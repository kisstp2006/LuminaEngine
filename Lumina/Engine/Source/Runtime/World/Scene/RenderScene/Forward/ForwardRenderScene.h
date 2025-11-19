#pragma once
#include "Core/Delegates/Delegate.h"
#include "Renderer/BindingCache.h"
#include "Renderer/TypedBuffer.h"
#include "World/Scene/RenderScene/MeshDrawCommand.h"
#include "World/Scene/RenderScene/RenderScene.h"


namespace Lumina
{
    class CWorld;
    class FForwardRenderScene : public IRenderScene
    {

        static constexpr int NumCascades = 4;

    public:
        FForwardRenderScene(CWorld* InWorld);
        
        void Init() override;
        void Shutdown() override;
        void RenderScene(FRenderGraph& RenderGraph, const FViewVolume& ViewVolume) override;
        void SetViewVolume(const FViewVolume& ViewVolume) override;
        void SwapchainResized(glm::vec2 NewSize);
        
        void CompileDrawCommands(FRenderGraph& RenderGraph) override;

        //~ Begin Render Passes
        void ResetPass(FRenderGraph& RenderGraph);
        void CullPass(FRenderGraph& RenderGraph, const FViewVolume& View);
        void DepthPrePass(FRenderGraph& RenderGraph, const FViewVolume& View);
        void DepthPyramidPass(FRenderGraph& RenderGraph);
        void ClusterBuildPass(FRenderGraph& RenderGraph, const FViewVolume& View);
        void LightCullPass(FRenderGraph& RenderGraph, const FViewVolume& View);
        void ClearShadowPass(FRenderGraph& RenderGraph);
        void PointShadowPass(FRenderGraph& RenderGraph);
        void SpotShadowPass(FRenderGraph& RenderGraph);
        void DirectionalShowPass(FRenderGraph& RenderGraph);
        void BasePass(FRenderGraph& RenderGraph, const FViewVolume& View);
        void TransparentPass(FRenderGraph& RenderGraph, const FViewVolume& View);
        void EnvironmentPass(FRenderGraph& RenderGraph);
        void BatchedLineDraw(FRenderGraph& RenderGraph);
        void ToneMappingPass(FRenderGraph& RenderGraph);
        void DebugDrawPass(FRenderGraph& RenderGraph);
        //~ End Render Passes

        void InitBuffers();
        void InitImages();
        void InitFrameResources();

        static FViewportState MakeViewportStateFromImage(const FRHIImage* Image);

        void CheckInstanceBufferResize(uint32 NumInstances);
        void CheckLightBufferResize(uint32 NumLights);
        
        FRHIImageRef GetRenderTarget() const override;
        ERenderSceneDebugFlags GetDebugMode() const override;
        void SetDebugMode(ERenderSceneDebugFlags Mode) override;
        FSceneRenderSettings& GetSceneRenderSettings() override;
        entt::entity GetEntityAtPixel(uint32 X, uint32 Y) const override;


        FDelegateHandle                     SwapchainResizedHandle;
        CWorld*                             World = nullptr;
        
        FSceneRenderSettings                RenderSettings;
        FSceneLightData                     LightData;

        /** Packed array of all light shadows in the scene */
        TArray<TVector<FLightShadow>, (uint32)ELightType::Num>    PackedShadows;

        FBindingCache                       BindingCache;

        FRHIViewportRef                     SceneViewport;
        
        FRHIInputLayoutRef                  VertexLayoutInput;
        FRHIInputLayoutRef                  PositionOnlyLayoutInput;
        FRHIInputLayoutRef                  SimpleVertexLayoutInput;

        FSceneGlobalData                    SceneGlobalData;

        FRHIBindingSetRef                   BasePassSet;
        FRHIBindingLayoutRef                BasePassLayout;

        FRHIBindingSetRef                   ToneMappingPassSet;
        FRHIBindingLayoutRef                ToneMappingPassLayout;
        
        FRHIBindingSetRef                   ShadowPassSet;
        FRHIBindingLayoutRef                ShadowPassLayout;

        FRHIBindingSetRef                   DebugPassSet;
        FRHIBindingLayoutRef                DebugPassLayout;

        FRHIBindingSetRef                   SSAOBlurPassSet;
        FRHIBindingLayoutRef                SSAOBlurPassLayout;
        
        FRHIBindingSetRef                   BindingSet;
        FRHIBindingLayoutRef                BindingLayout;

        FRHIBindingSetRef                   MeshCullSet;
        FRHIBindingLayoutRef                MeshCullLayout;

        FRHIBindingSetRef                   ClusterBuildSet;
        FRHIBindingLayoutRef                ClusterBuildLayout;

        FRHIBindingSetRef                   LightCullSet;
        FRHIBindingLayoutRef                LightCullLayout;

        FRHITypedVertexBuffer<FSimpleElementVertex> SimpleVertexBuffer;
        TRenderVector<FSimpleElementVertex>         SimpleVertices;
        FRHIBindingLayoutRef                        SimplePassLayout;

        FRHIBufferRef                               CullDataBuffer;
        FRHIBufferRef                               ClusterBuffer;
        FRHIBufferRef                               SceneDataBuffer;
        FRHIBufferRef                               InstanceDataBuffer;
        FRHIBufferRef                               InstanceMappingBuffer;
        FRHIBufferRef                               LightDataBuffer;
        FRHIBufferRef                               IndirectDrawBuffer;
        
        FShadowAtlas                        ShadowAtlas;

        FRHIImageRef                        HDRRenderTarget;
        FRHIImageRef                        CascadedShadowMap;
        FRHIImageRef                        DepthAttachment;
        FRHIImageRef                        DepthPyramid;
        FRHIImageRef                        PickerImage;
        
        ERenderSceneDebugFlags              DebugVisualizationMode;

        TArray<FShadowCascade, NumCascades>           ShadowCascades;
        
        /** Packed array of per-instance data */
        TRenderVector<FInstanceData>                  InstanceData;

        
        FMeshPass DepthMeshPass;
        FMeshPass OpaqueMeshPass;
        FMeshPass TranslucentMeshPass;
        FMeshPass ShadowMeshPass;
        
        /** Packed array of all cached mesh draw commands */
        TRenderVector<FMeshDrawCommand>             DrawCommands;

        /** Packed indirect draw arguments, gets sent directly to the GPU */
        TRenderVector<FDrawIndexedIndirectArguments>  IndirectDrawArguments;
    };
}
