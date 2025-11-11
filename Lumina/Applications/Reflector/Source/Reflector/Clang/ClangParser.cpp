#include "ClangParser.h"
#include <fstream>
#include <clang-c/Index.h>
#include "EASTL/fixed_vector.h"
#include "Visitors/ClangTranslationUnit.h"



namespace Lumina::Reflection
{

    static const char* const GIncludePaths[] =
    {
        "/Lumina/Engine/",
        "/Lumina/Engine/Source",
        "/Lumina/Engine/ThirdParty/",
        "/Lumina/Engine/Source/Runtime",

        "/Lumina/Engine/ThirdParty/spdlog/include",
        "/Lumina/Engine/ThirdParty/GLFW/include",
        "/Lumina/Engine/ThirdParty/imgui",
        "/Lumina/Engine/ThirdParty/vk-bootstrap",
        "/Lumina/Engine/ThirdParty/VulkanMemoryAllocator",
        "/Lumina/Engine/ThirdParty/fastgltf/include",
        "/Lumina/Engine/ThirdParty/stb_image",
        "/Lumina/Engine/ThirdParty/meshoptimizer/src",
        "/Lumina/Engine/ThirdParty/EnkiTS/src",
        "/Lumina/Engine/ThirdParty/SPIRV-Reflect",
        "/Lumina/Engine/ThirdParty/json/include",
        "/Lumina/Engine/ThirdParty/entt/single_include",
        "/Lumina/Engine/ThirdParty/ImGuizmo",
        "/Lumina/Engine/ThirdParty/EA/EASTL/include",
        "/Lumina/Engine/ThirdParty/EA/EABase/include/Common",
        "/Lumina/Engine/ThirdParty/rpmalloc",
        "/Lumina/Engine/ThirdParty/xxhash",
        "/Lumina/Engine/ThirdParty/tracy/public",
        "/External/Physx/physx/include",
    };

    eastl::string ToLower(const eastl::string& Str)
    {
        eastl::string Lower;
        Lower.resize(Str.size());
        eastl::transform(Lower.begin(), Lower.end(), Lower.begin(), ::tolower);
        return Lower;
    }
    
    FClangParser::FClangParser()
    {
    }

    bool FClangParser::Parse(const eastl::string& SolutionPath, eastl::vector<FReflectedHeader>& Headers, FReflectedProject& Project)
    {
        namespace fs = std::filesystem;
        
        if (Project.TranslationUnit == nullptr)
        {
            auto start = std::chrono::high_resolution_clock::now();
    
            ParsingContext.Solution = FProjectSolution(SolutionPath.c_str());
            ParsingContext.Project = &Project;
    
            const eastl::string ProjectReflectionDirectory = ParsingContext.Solution.GetParentPath() + "/Intermediates/Reflection/" + Project.Name;

            if (!fs::exists(ProjectReflectionDirectory.c_str()))
            {
                fs::create_directories(ProjectReflectionDirectory.c_str());
            }

#if 0
            eastl::vector<fs::path> ReflectionHeadersToDelete;

            eastl::hash_set<eastl::string> CurrentHeaderNames;
            for (const FReflectedHeader& Header : Headers)
            {
                CurrentHeaderNames.insert(ToLower(Header.FileName));
            }

            for (const auto& Entry : fs::directory_iterator(ProjectReflectionDirectory.c_str()))
            {
                if (Entry.is_regular_file())
                {
                    fs::path FilePath = Entry.path();
                    eastl::string FileStem = FilePath.stem().string().c_str();
                    
                    const eastl::string Suffix = ".generated";
                    if (FileStem.size() > Suffix.size() && FileStem.compare(FileStem.size() - Suffix.size(), Suffix.size(), Suffix) == 0)
                    {
                        FileStem.resize(FileStem.size() - Suffix.size());
                    }

                    FileStem = ToLower(FileStem);

                    if (CurrentHeaderNames.find(FileStem) == CurrentHeaderNames.end())
                    {
                        ReflectionHeadersToDelete.push_back(FilePath);
                    }
                }
            }

            for (const fs::path& FileToDelete : ReflectionHeadersToDelete)
            {
                std::filesystem::remove(FileToDelete);
            }

            eastl::vector<FReflectedHeader> HeadersToParse;
            if (std::filesystem::exists(ProjectReflectionDirectory.c_str()))
            {
                for (const FReflectedHeader& Header : Headers)
                {
                    fs::path PossibleReflectionFilePath = fs::path(ProjectReflectionDirectory.c_str()) / eastl::string(Header.FileName + ".generated.h").c_str();
                    
                    if (std::filesystem::exists(PossibleReflectionFilePath))
                    {
                        auto HeaderWriteTime = fs::last_write_time(Header.HeaderPath.c_str());
                        auto ReflectionWriteTime = fs::last_write_time(PossibleReflectionFilePath.c_str());

                        if (HeaderWriteTime > ReflectionWriteTime)
                        {
                            HeadersToParse.push_back(Header);
                        }
                    }
                    else
                    {
                        HeadersToParse.push_back(Header);
                    }
                }
            }


            if (HeadersToParse.empty())
            {
                return false;
            }
#endif
            
            for (const auto& Entry : std::filesystem::directory_iterator(ProjectReflectionDirectory.c_str()))
            {
                std::error_code ec;
                std::filesystem::remove_all(Entry, ec);
                if (ec)
                {
                    std::cout << "Failed to delete: " << Entry.path() << " (" << ec.message() << ")\n";
                }
            }
            
            const eastl::string AmalgamationPath = ProjectReflectionDirectory + "/ReflectHeaders.h";
            
            std::ofstream AmalgamationFile(AmalgamationPath.c_str());
            AmalgamationFile << "#pragma once\n\n";

            for (const FReflectedHeader& Header : Headers)
            {
                AmalgamationFile << "#include \"" << Header.HeaderPath.c_str() << "\"\n";
                ParsingContext.NumHeadersReflected++;
            }
    
            AmalgamationFile.close();
    
            eastl::vector<eastl::string> FullIncludePaths;
            eastl::fixed_vector<const char*, 32> clangArgs;
    
            eastl::string PrjPath = Project.ParentPath + "/Source/";
            FullIncludePaths.push_back("-I" + PrjPath);
            clangArgs.push_back(FullIncludePaths.back().c_str());
    
            eastl::string LuminaDirectory = std::getenv("LUMINA_DIR");
            if (!LuminaDirectory.empty() && LuminaDirectory.back() == '/' )
            {
                LuminaDirectory.pop_back();
            }
        
            for (const char* Path : GIncludePaths)
            {
                eastl::string FullPath = LuminaDirectory + Path;
                FullIncludePaths.emplace_back("-I" + FullPath);
                clangArgs.emplace_back(FullIncludePaths.back().c_str());
                if (!std::filesystem::exists(FullPath.c_str()))
                {
                    ParsingContext.LogError("Invalid include path: %s", FullPath.c_str());
                    return false;
                }
            }
        
            clangArgs.emplace_back("-x");
            clangArgs.emplace_back("c++");
            clangArgs.emplace_back("-std=c++23");
            clangArgs.emplace_back("-O0");
        
            clangArgs.emplace_back("-D");
            clangArgs.emplace_back("REFLECTION_PARSER");
    
            clangArgs.emplace_back("-D");
            clangArgs.emplace_back("NDEBUG");
    
            clangArgs.emplace_back("-fsyntax-only");
            clangArgs.emplace_back("-fparse-all-comments");
            clangArgs.emplace_back("-fms-extensions");
            clangArgs.emplace_back("-fms-compatibility");
            clangArgs.emplace_back("-Wfatal-errors=0");
            clangArgs.emplace_back("-w");
        
            clangArgs.emplace_back("-ferror-limit=1000000000");
        
            clangArgs.emplace_back("-Wno-multichar");
            clangArgs.emplace_back("-Wno-deprecated-builtins");
            clangArgs.emplace_back("-Wno-unknown-warning-option");
            clangArgs.emplace_back("-Wno-return-type-c-linkage");
            clangArgs.emplace_back("-Wno-c++98-compat-pedantic");
            clangArgs.emplace_back("-Wno-gnu-folding-constant");
            clangArgs.emplace_back("-Wno-vla-extension-static-assert");
            
            // REMOVED: Don't use these, they will break parsing!
            // clangArgs.emplace_back("-nostdinc");
            // clangArgs.emplace_back("-nostdinc++"); 
            // clangArgs.emplace_back("-nobuiltininc");
    
            clangArgs.emplace_back("-fno-spell-checking");
            clangArgs.emplace_back("-fno-delayed-template-parsing");
    
            Project.ClangIndex = clang_createIndex(0, 0);
            
            constexpr uint32_t ClangOptions = 
                CXTranslationUnit_DetailedPreprocessingRecord |
                CXTranslationUnit_SkipFunctionBodies | 
                CXTranslationUnit_KeepGoing |
                CXTranslationUnit_IgnoreNonErrorsFromIncludedFiles;
        
            CXErrorCode Result = clang_parseTranslationUnit2(
                Project.ClangIndex,
                AmalgamationPath.c_str(),
                clangArgs.data(),
                clangArgs.size(),
                nullptr,
                0,
                ClangOptions,
                &Project.TranslationUnit);
    
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration = end - start;
            std::cout << "[Reflection] Parsed " << Project.Name.c_str() << " in " << duration.count() << "s\n";
        
            if (Result != CXError_Success)
            {
                switch (Result)
                {
                case CXError_Failure:
                    ParsingContext.LogError("Clang Unknown failure");
                    break;
    
                case CXError_Crashed:
                    ParsingContext.LogError("Clang crashed");
                    break;
    
                case CXError_InvalidArguments:
                    ParsingContext.LogError("Clang Invalid arguments");
                    break;
    
                case CXError_ASTReadError:
                    ParsingContext.LogError("Clang AST read error");
                    break;
                }
    
                clang_disposeIndex(Project.ClangIndex);
                return false;
            }
        }
        
        ParsingContext.Project = &Project;
        CXCursor Cursor = clang_getTranslationUnitCursor(Project.TranslationUnit);
        clang_visitChildren(Cursor, VisitTranslationUnit, &ParsingContext);
        
        return true;
    }

    void FClangParser::Dispose(FReflectedProject& Project)
    {
        if (Project.TranslationUnit)
        {
            clang_disposeTranslationUnit(Project.TranslationUnit);
            Project.TranslationUnit = nullptr;
        }
        if (Project.ClangIndex)
        {
            clang_disposeIndex(Project.ClangIndex);
            Project.ClangIndex = nullptr;
        }
    }
}
