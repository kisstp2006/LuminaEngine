#pragma once

#include "imgui.h"
#include "ImGuiX.h"
#include "Renderer/RHIFwd.h"
#include "Subsystems/Subsystem.h"

namespace Lumina
{
    class FRenderGraph;
    class FRenderManager;
}

namespace Lumina
{
    class IImGuiRenderer
    {
    public:
        
        virtual ~IImGuiRenderer() = default;

        virtual void Initialize();
        virtual void Deinitialize();
        
        void StartFrame(const FUpdateContext& UpdateContext);
        void EndFrame(const FUpdateContext& UpdateContext, FRenderGraph& RenderGraph);
        
        virtual void OnStartFrame(const FUpdateContext& UpdateContext) = 0;
        virtual void OnEndFrame(const FUpdateContext& UpdateContext, FRenderGraph& RenderGraph) = 0;

        virtual ImTextureID GetOrCreateImTexture(FRHIImageRef Image) = 0;
        virtual void DestroyImTexture(ImTextureRef Image) = 0;

        virtual void DrawRenderDebugInformationWindow(bool* bOpen, const FUpdateContext& Context) = 0;
        
    protected:

        
    };
}
