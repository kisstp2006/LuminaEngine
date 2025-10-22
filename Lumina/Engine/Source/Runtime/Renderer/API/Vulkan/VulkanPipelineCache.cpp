#include "VulkanPipelineCache.h"

#include "VulkanResources.h"
#include "Core/Profiler/Profile.h"

namespace Lumina
{

    FRHIGraphicsPipelineRef FVulkanPipelineCache::GetOrCreateGraphicsPipeline(FVulkanDevice* Device, const FGraphicsPipelineDesc& InDesc)
    {
        LUMINA_PROFILE_SCOPE();

        SIZE_T Hash = Hash::GetHash(InDesc);
        if (GraphicsPipelines.find(Hash) != GraphicsPipelines.end())
        {
            return GraphicsPipelines.at(Hash);
        }
        
        auto NewPipeline = TRefCountPtr<FVulkanGraphicsPipeline>::Create(Device, InDesc);
        

        GraphicsPipelines.emplace(Hash, NewPipeline);
        return NewPipeline;
    }

    FRHIComputePipelineRef FVulkanPipelineCache::GetOrCreateComputePipeline(FVulkanDevice* Device, const FComputePipelineDesc& InDesc)
    {
        LUMINA_PROFILE_SCOPE();

        SIZE_T Hash = Hash::GetHash(InDesc);
        if (ComputePipelines.find(Hash) != ComputePipelines.end())
        {
            return ComputePipelines.at(Hash);
        }
        
        auto NewPipeline = MakeRefCount<FVulkanComputePipeline>(Device, InDesc);

        FScopeLock Lock(ShaderMutex);
        ComputePipelines.emplace(Hash, NewPipeline);
        return NewPipeline;
    }

    void FVulkanPipelineCache::PostShaderRecompiled(const FRHIShader* Shader)
    {
        FScopeLock Lock(ShaderMutex);
        if (Shader)
        {
            const FString& ShaderName = Shader->GetShaderHeader().DebugName;

            for (auto it = GraphicsPipelines.begin(); it != GraphicsPipelines.end(); )
            {
                auto& Desc = it->second->GetDesc();

                bool bMatches =
                    (Desc.VS && Desc.VS->GetShaderHeader().DebugName == ShaderName) ||
                    (Desc.PS && Desc.PS->GetShaderHeader().DebugName == ShaderName) ||
                    (Desc.GS && Desc.GS->GetShaderHeader().DebugName == ShaderName);

                if (bMatches)
                {
                    it = GraphicsPipelines.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            for (auto it = ComputePipelines.begin(); it != ComputePipelines.end(); )
            {
                auto& Pipeline = it->second;
                if (Pipeline->GetDesc().CS && Pipeline->GetDesc().CS->GetShaderHeader().DebugName == ShaderName)
                {
                    it = ComputePipelines.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
        else
        {
            GraphicsPipelines.clear();
            ComputePipelines.clear();
        }
    }
    
    void FVulkanPipelineCache::ReleasePipelines()
    {
        FScopeLock Lock(ShaderMutex);

        GraphicsPipelines.clear();
        ComputePipelines.clear();
    }
}
