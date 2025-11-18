#include "RenderManager.h"

#if LUMINA_RENDERER_VULKAN
#include "API/Vulkan/VulkanRenderContext.h"
#include "Tools/UI/ImGui/Vulkan/VulkanImGuiRender.h"
#endif

#include "RHIGlobals.h"
#include "Core/Profiler/Profile.h"
#include "RenderGraph/RenderGraph.h"
#include "Tools/UI/ImGui/ImGuiRenderer.h"

namespace Lumina
{
    void FRenderManager::Initialize()
    {
        GRenderContext = Memory::New<FVulkanRenderContext>();
        GRenderContext->Initialize(FRenderContextDesc{false});

        #if WITH_DEVELOPMENT_TOOLS
        ImGuiRenderer = Memory::New<FVulkanImGuiRender>();
        ImGuiRenderer->Initialize();
        #endif
    }

    void FRenderManager::Deinitialize()
    {
        #if WITH_DEVELOPMENT_TOOLS

        ImGuiRenderer->Deinitialize();
        Memory::Delete(ImGuiRenderer);
        ImGuiRenderer = nullptr;
        #endif

        
        GRenderContext->Deinitialize();
        Memory::Delete(GRenderContext);
        GRenderContext = nullptr;
    }

    void FRenderManager::FrameStart(const FUpdateContext& UpdateContext)
    {
        LUMINA_PROFILE_SCOPE();
        
        GRenderContext->FrameStart(UpdateContext, CurrentFrameIndex);

        #if WITH_DEVELOPMENT_TOOLS
        ImGuiRenderer->StartFrame(UpdateContext);
        #endif
    }

    void FRenderManager::FrameEnd(const FUpdateContext& UpdateContext, FRenderGraph& RenderGraph)
    {
        LUMINA_PROFILE_SCOPE();
        
        #if WITH_DEVELOPMENT_TOOLS
        ImGuiRenderer->EndFrame(UpdateContext, RenderGraph);
        #endif

        // Internally executes the render graph.
        GRenderContext->FrameEnd(UpdateContext, RenderGraph);

        
        GRenderContext->FlushPendingDeletes();
        
        CurrentFrameIndex = (CurrentFrameIndex + 1) % FRAMES_IN_FLIGHT;
    }
}
