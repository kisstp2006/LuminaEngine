#pragma once
#include "Subsystems/Subsystem.h"
#include "Lumina.h"
#include "RHIFwd.h"
#include "Core/Delegates/Delegate.h"


namespace Lumina
{
    class FRenderGraph;
    class IImGuiRenderer;
    class IRenderContext;
}

namespace Lumina
{
    class FRenderManager : public ISubsystem
    {
    public:

        static TMulticastDelegate<void, glm::vec2> OnSwapchainResized;


        void Initialize() override;
        void Deinitialize() override;

        void FrameStart(const FUpdateContext& UpdateContext);
        void FrameEnd(const FUpdateContext& UpdateContext, FRenderGraph& RenderGraph);

        void SwapchainResized(glm::vec2 NewSize);


        #if WITH_DEVELOPMENT_TOOLS
        IImGuiRenderer* GetImGuiRenderer() const { return ImGuiRenderer; }
        #endif

        uint32 GetCurrentFrameIndex() const { return CurrentFrameIndex; }
        
    private:

        #if WITH_DEVELOPMENT_TOOLS
        IImGuiRenderer*     ImGuiRenderer = nullptr;
        #endif
        
        uint8               CurrentFrameIndex = 0;
        
    };
}
