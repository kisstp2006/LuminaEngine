#include "ShaderCompiler.h"

#include "RenderResource.h"
#include "Core/Serialization/MemoryArchiver.h"
#include "Core/Threading/Thread.h"
#include "Memory/Memory.h"
#include "Paths/Paths.h"
#include "Platform/Filesystem/FileHelper.h"
#include "shaderc/shaderc.hpp"
#include "TaskSystem/TaskSystem.h"
#include "SPIRV-Reflect/spirv_reflect.h"

namespace Lumina
{
    
    class FShaderCIncluder : public shaderc::CompileOptions::IncluderInterface
    {
        shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth) override
        {
            FString RequestingPath = requesting_source;
            if (include_depth > 1)
            {
                RequestingPath = Paths::GetEngineShadersDirectory() + "/" + RequestingPath;
            }
            FString IncludePath = Paths::Parent(RequestingPath) + "/" + FString(requested_source);
            
            FString ShaderData;
            if (FileHelper::LoadFileIntoString(ShaderData, IncludePath))
            {
                auto* result = Memory::New<shaderc_include_result>();
                result->source_name = requested_source;
                result->source_name_length = strlen(requested_source);
                result->content = Memory::NewArray<char>(ShaderData.size() + 1);
                result->content_length = ShaderData.size();

                memcpy((void*)result->content, ShaderData.data(), ShaderData.size() + 1);
                return result;
            }

            LOG_ERROR("Failed to find shader include at path: {}", IncludePath);
            return nullptr;
        }

        void ReleaseInclude(shaderc_include_result* data) override
        {
            Memory::DeleteArray(const_cast<char*>(data->content));
            Memory::Delete(data);
        }
    };

    void FSpirVShaderCompiler::ReflectSpirv(TSpan<uint32> SpirV, FShaderReflection& Reflection, bool bReflectFull)
    {
        SpvReflectShaderModule Module;
        SpvReflectResult Result = spvReflectCreateShaderModule(SpirV.size_bytes(), SpirV.data(), &Module);
        if (Result != SPV_REFLECT_RESULT_SUCCESS)
        {
            LOG_ERROR("Failed to create SPIR-V Reflect Module!");
            return;
        }
        
        switch (Module.shader_stage)  // NOLINT(clang-diagnostic-switch)
        {
        case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
            Reflection.ShaderType = ERHIShaderType::Vertex;
            break;
        case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
            Reflection.ShaderType = ERHIShaderType::Fragment;
            break;
        case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
            Reflection.ShaderType = ERHIShaderType::Compute;
            break;
        }

        if (!bReflectFull)
        {
            return;
        }

        uint32 NumDescriptorSets = 0;
        Result = spvReflectEnumerateDescriptorSets(&Module, &NumDescriptorSets, nullptr);
        if (Result != SPV_REFLECT_RESULT_SUCCESS)
        {
            LOG_ERROR("Failed to enumerate descriptor sets (count).");
            return;
        }

        TVector<SpvReflectDescriptorSet*> ReflectSets(NumDescriptorSets);
        Result = spvReflectEnumerateDescriptorSets(&Module, &NumDescriptorSets, ReflectSets.data());
        if (Result != SPV_REFLECT_RESULT_SUCCESS)
        {
            LOG_ERROR("Failed to enumerate descriptor sets (data).");
            return;
        }

        
        for (uint32 SetIndex = 0; SetIndex < NumDescriptorSets; ++SetIndex)
        {
            SpvReflectDescriptorSet* ReflectSet = ReflectSets[SetIndex];

            for (uint32 BindingIndex = 0; BindingIndex < ReflectSet->binding_count; ++BindingIndex)
            {
                SpvReflectDescriptorBinding* Binding = ReflectSet->bindings[BindingIndex];

                Reflection.Bindings.push_back();
                FShaderBinding& NewBinding = Reflection.Bindings.back();
                
                NewBinding.Name = Binding->name ? Binding->name : "";
                NewBinding.Set = Binding->set;
                NewBinding.Binding = Binding->binding;

                if (Binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                {
                    NewBinding.Type = ERHIBindingResourceType::Buffer_CBV;
                    NewBinding.Size = Binding->block.size;
                }
                else if (Binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                {
                    NewBinding.Type = ERHIBindingResourceType::Buffer_UAV;
                    NewBinding.Size = Binding->block.size;
                }
                else if (Binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                {
                    NewBinding.Type = ERHIBindingResourceType::Texture_SRV;
                }
                else if (Binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER)
                {
                    NewBinding.Type = ERHIBindingResourceType::Sampler;
                }
                else if (Binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                {
                    NewBinding.Type = ERHIBindingResourceType::Texture_UAV;
                }
            }
        }
        
        spvReflectDestroyShaderModule(&Module);
    }
    
    
    bool FSpirVShaderCompiler::CompileShaderPath(FString ShaderPath, const FShaderCompileOptions& CompileOptions, CompletedFunc OnCompleted)
    {
        TVector<FString> ShaderPaths = { Memory::Move(ShaderPath) };
        TVector<FShaderCompileOptions> Options = { CompileOptions };
        TVector<CompletedFunc> Callbacks = { Memory::Move(OnCompleted) };

        return CompileShaderPaths(TSpan<FString>(ShaderPaths), TSpan<FShaderCompileOptions>(Options), OnCompleted);
    }

    bool FSpirVShaderCompiler::CompileShaderPaths(TSpan<FString> ShaderPaths, TSpan<FShaderCompileOptions> CompileOptions, CompletedFunc OnCompleted)
    {
        LUM_ASSERT(ShaderPaths.size() == CompileOptions.size())
    
        uint32 NumShaders = (uint32)ShaderPaths.size();
        if (NumShaders == 0)
        {
            return false;
        }
        
        PendingTasks.fetch_add(NumShaders, std::memory_order_relaxed);

        LOG_INFO("Starting Shader Task Swarm - Num: {}", NumShaders);
        
        // Capture copies for thread safety
        TVector<FString> Paths(ShaderPaths.begin(), ShaderPaths.end());
        TVector<FShaderCompileOptions> Options(CompileOptions.begin(), CompileOptions.end());
    
        Task::AsyncTask(NumShaders, [this,
            Paths = Memory::Move(Paths),
            Options = Memory::Move(Options),
            Callback = Memory::Move(OnCompleted)] (uint32 Start, uint32 End, uint32 Thread)
        {
            shaderc::CompileOptions CompileOpts;
            CompileOpts.SetIncluder(std::make_unique<FShaderCIncluder>());
            CompileOpts.SetOptimizationLevel(shaderc_optimization_level_performance);
            CompileOpts.SetGenerateDebugInfo();
            CompileOpts.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    
            for (uint32 i = Start; i < End; ++i)
            {
                auto CompileStart = std::chrono::high_resolution_clock::now();

                const FString& Path = Paths[i];
                FString Filename = Paths::FileName(Path);
                const FShaderCompileOptions& Opt = Options[i];
                
                for (const FString& Macro : Opt.MacroDefinitions)
                {
                    CompileOpts.AddMacroDefinition(Macro.c_str());
                }
    
                FString RawShaderString;
                if (!FileHelper::LoadFileIntoString(RawShaderString, Path))
                {
                    LOG_ERROR("Failed to load shader: {0}", Path);
                    PendingTasks.fetch_sub(1, std::memory_order_acq_rel);
                    continue;
                }
    
                auto Preprocessed = Compiler.PreprocessGlsl(
                    RawShaderString.c_str(),
                    RawShaderString.size(),
                    shaderc_glsl_infer_from_source,
                    Path.c_str(),
                    CompileOpts);
    
                if (Preprocessed.GetCompilationStatus() != shaderc_compilation_status_success)
                {
                    LOG_ERROR("Preprocessing failed: {0} - {1}", Path, Preprocessed.GetErrorMessage());
                    PendingTasks.fetch_sub(1, std::memory_order_acq_rel);
                    continue;
                }
    
                FString PreprocessedShader(Preprocessed.begin(), Preprocessed.end());
    
                auto CompileResult = Compiler.CompileGlslToSpv(
                    PreprocessedShader.c_str(),
                    PreprocessedShader.size(),
                    shaderc_glsl_infer_from_source,
                    Path.c_str(),
                    CompileOpts);
    
                if (CompileResult.GetCompilationStatus() != shaderc_compilation_status_success)
                {
                    LOG_ERROR("Compilation failed: {0} - {1}", Path, CompileResult.GetErrorMessage());
                    continue;
                }
    
                TVector<uint32> Binaries(CompileResult.begin(), CompileResult.end());
                if (Binaries.empty())
                {
                    LOG_ERROR("Shader compiled to empty SPIR-V: {0}", Path);
                    PendingTasks.fetch_sub(1, std::memory_order_acq_rel);
                    continue;
                }

                
                FShaderHeader Shader;
                Shader.Binaries = Memory::Move(Binaries);
                Shader.DebugName = Filename;
                
                ReflectSpirv(Shader.Binaries, Shader.Reflection, Options[i].bGenerateReflectionData);

                auto CompileEnd = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> DurationMs = CompileEnd - CompileStart;
                
                LOG_TRACE("Compiled shader {0} in {1:.2f} ms (Thread {2})", Filename, DurationMs.count(), Thread);
                
                PendingTasks.fetch_sub(1, std::memory_order_acq_rel);
                
                Callback(Memory::Move(Shader));
            }
        }, ETaskPriority::High);
    
        return true;
    }

    FSpirVShaderCompiler::FSpirVShaderCompiler()
    {
    }

    void FSpirVShaderCompiler::Initialize()
    {
        
    }

    void FSpirVShaderCompiler::Shutdown()
    {
        Flush();
    }

    bool FSpirVShaderCompiler::CompilerShaderRaw(FString ShaderString, const FShaderCompileOptions& CompileOptions, CompletedFunc OnCompleted)
    {
        PendingTasks.fetch_add(1, std::memory_order_relaxed);

        Task::AsyncTask(1, [this,
            ShaderString = Memory::Move(ShaderString),
            CompileOptions = Memory::Move(CompileOptions),
            Callback = Memory::Move(OnCompleted)]
            (uint32 Start, uint32 End, uint32 ThreadNum_)
        {
            LOG_TRACE("Compiling Raw Shader - Thread: {0}", Threading::GetThreadID());

            shaderc::CompileOptions Options;
            Options.SetIncluder(std::make_unique<FShaderCIncluder>());
            Options.SetOptimizationLevel(shaderc_optimization_level_performance);
            Options.SetGenerateDebugInfo();
            Options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
            
            FString VertexPath = Paths::GetEngineResourceDirectory() + "/Shaders/GeometryPass.vert";
            
            TVector<uint32> Binaries;
            for (const FString& Macro : CompileOptions.MacroDefinitions)
            {
                Options.AddMacroDefinition(Macro.c_str());
            }
             
            auto Preprocessed = Compiler.PreprocessGlsl(ShaderString.c_str(),
                                                        ShaderString.size(),
                                                        shaderc_glsl_infer_from_source,
                                                        VertexPath.c_str(), Options);
    
            if (Preprocessed.GetCompilationStatus() != shaderc_compilation_status_success)
            {
                LOG_ERROR("Preprocessing failed: - {}", Preprocessed.GetErrorMessage());
                return;
            }
    
            FString PreprocessedShader(Preprocessed.begin(), Preprocessed.end());
    
            auto CompileResult = Compiler.CompileGlslToSpv(PreprocessedShader.c_str(),
                                                           PreprocessedShader.size(),
                                                           shaderc_glsl_infer_from_source,
                                                           VertexPath.c_str(), Options);
    
            if (CompileResult.GetCompilationStatus() != shaderc_compilation_status_success)
            {
                LOG_ERROR("Compilation failed: - {}", CompileResult.GetErrorMessage());
                return;
            }
    
            Binaries.assign(CompileResult.begin(), CompileResult.end());
            
            if (Binaries.empty())
            {
                LOG_ERROR("Shader compiled to empty SPIR-V");
                return;
            }

            
            FShaderHeader Shader;
            Shader.DebugName = "RawShader";
            Shader.Binaries = Memory::Move(Binaries);
            
            ReflectSpirv(Shader.Binaries, Shader.Reflection, true);

            PendingTasks.fetch_sub(1, std::memory_order_acq_rel);

            Callback(Memory::Move(Shader));
            
        });
        
        return true;
    }
}
