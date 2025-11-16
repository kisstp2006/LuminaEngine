#pragma once
#include <entt/entt.hpp>

#include "SceneRenderTypes.h"
#include "Platform/GenericPlatform.h"
#include "Renderer/RHIFwd.h"
#include "World/Scene/SceneInterface.h"


namespace Lumina
{
    class FRenderGraph;
    class FViewVolume;

    class IRenderScene : public ISceneInterface
    {
    public:

        virtual ~IRenderScene() = default;
        
        LUMINA_API virtual void RenderScene(FRenderGraph& RenderGraph, const FViewVolume& ViewVolume) = 0;
        LUMINA_API virtual void SetViewVolume(const FViewVolume& ViewVolume) = 0;
        LUMINA_API virtual void CompileDrawCommands(FRenderGraph& RenderGraph) = 0;
        LUMINA_API virtual FRHIImageRef GetRenderTarget() const = 0;
        LUMINA_API virtual ERenderSceneDebugFlags GetDebugMode() const = 0;
        LUMINA_API virtual void SetDebugMode(ERenderSceneDebugFlags Mode) = 0;
        LUMINA_API virtual FSceneRenderSettings& GetSceneRenderSettings() = 0;

        LUMINA_API virtual entt::entity GetEntityAtPixel(uint32 X, uint32 Y) const = 0;
        
    };
}
