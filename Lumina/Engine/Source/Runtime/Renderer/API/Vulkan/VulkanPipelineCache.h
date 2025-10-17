#pragma once
#include "Containers/Array.h"
#include "Core/Threading/Thread.h"
#include "Renderer/RHIFwd.h"


namespace Lumina
{
    class IVulkanShader;
    struct FComputePipelineDesc;
    struct FGraphicsPipelineDesc;
    class FVulkanDevice;
}


namespace Lumina
{

    class FVulkanPipelineCache
    {
    public:

        struct FShaderPipelineTracker
        {
            TVector<FName> Shaders;
        };

        FRHIGraphicsPipelineRef GetOrCreateGraphicsPipeline(FVulkanDevice* Device, const FGraphicsPipelineDesc& InDesc);
        FRHIComputePipelineRef GetOrCreateComputePipeline(FVulkanDevice* Device, const FComputePipelineDesc& InDesc);

        void PostShaderRecompiled(FRHIShader* Shader);
        void ReleasePipelines();
        
    private:

        FMutex ShaderMutex;
        THashMap<SIZE_T, FRHIGraphicsPipelineRef>   GraphicsPipelines;
        THashMap<SIZE_T, FRHIComputePipelineRef>    ComputePipelines;
        
    };
}
