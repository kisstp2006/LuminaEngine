#pragma once

#include "entt/entt.hpp"
#include "Renderer/RenderResource.h"
#include "Renderer/RenderTypes.h"
#include "Renderer/TypedBuffer.h"
#include "World/Scene/RenderScene/MeshDrawCommand.h"
#include "World/Scene/RenderScene/RenderScene.h"
#include "World/Scene/RenderScene/SceneRenderTypes.h"


namespace Lumina
{
    struct FSceneGlobalData;
    struct FSceneRenderSettings;
    struct FScenePrimitive;
    struct FStaticMeshScenePrimitive;
    class FRenderGraph;
    class CWorld;
    class FUpdateContext;
    class CStaticMesh;
    class FPrimitiveDrawManager;
    struct FMaterialTexturesData;
    class CMaterialInstance;
    class FRenderer;
}


namespace Lumina
{
    
    
    class
    LUM_DEPRECATED(0.0.1, "Forward+ (Clustered) is now the default renderer in Lumina")
    FDeferredRenderScene : public IRenderScene
    {
    public:

        static constexpr int NumCascades = 4;

        friend struct FMeshBatch;
        
        FDeferredRenderScene(CWorld* InWorld);

        void Init() override;
        void Shutdown() override;
        
        void RenderScene(FRenderGraph& RenderGraph, const FViewVolume& ViewVolume) override;
    
        FRHIImageRef GetRenderTarget() const override { return SceneViewport->GetRenderTarget(); }
        FSceneGlobalData* GetSceneGlobalData() { return &SceneGlobalData; }

        FSceneRenderSettings& GetSceneRenderSettings() override { return RenderSettings; }
        
        ERenderSceneDebugFlags GetDebugMode() const override { return DebugVisualizationMode; }
        void SetDebugMode(ERenderSceneDebugFlags Mode) override { DebugVisualizationMode = Mode; }


        entt::entity GetEntityAtPixel(uint32 X, uint32 Y) const override;

    protected:

        void SetViewVolume(const FViewVolume& ViewVolume) override;

        
        /** Compiles all renderers from the world into draw commands for dispatch */
        void CompileDrawCommands(FRenderGraph& RenderGraph) override;

        //~ Begin Render Passes
        void ResetPass(FRenderGraph& RenderGraph);
        void CullPass(FRenderGraph& RenderGraph, const FViewVolume& View);
        void DepthPrePass(FRenderGraph& RenderGraph, const FViewVolume& View);
        void ClusterBuildPass(FRenderGraph& RenderGraph, const FViewVolume& View);
        void LightCullPass(FRenderGraph& RenderGraph, const FViewVolume& View);
        void PointShadowPass(FRenderGraph& RenderGraph);
        void SpotShadowPass(FRenderGraph& RenderGraph);
        void DirectionalShowPass(FRenderGraph& RenderGraph);
        void GBufferPass(FRenderGraph& RenderGraph, const FViewVolume& View);
        void SSAOPass(FRenderGraph& RenderGraph);
        void EnvironmentPass(FRenderGraph& RenderGraph);
        void DeferredLightingPass(FRenderGraph& RenderGraph);
        void BatchedLineDraw(FRenderGraph& RenderGraph);
        void ToneMappingPass(FRenderGraph& RenderGraph);
        void DebugDrawPass(FRenderGraph& RenderGraph);
        //~ End Render Passes

        static FViewportState MakeViewportStateFromImage(const FRHIImage* Image);

        void CheckInstanceBufferResize(uint32 NumInstances);
        void CheckLightBufferResize(uint32 NumLights);
        
        void InitResources();
        void InitBuffers();
        void CreateImages();
        void OnSwapchainResized();
        void GrowBufferIfNeeded(FRHIBufferRef& Buffer, SIZE_T DesiredSize, SIZE_T Padding);
    
    private:

        CWorld*                             World = nullptr;
        
        FSceneRenderSettings                RenderSettings;
        FSceneLightData                     LightData;

        FRHIViewportRef                     SceneViewport;
        
        FRHIInputLayoutRef                  VertexLayoutInput;
        FRHIInputLayoutRef                  PositionOnlyLayoutInput;
        FRHIInputLayoutRef                  SimpleVertexLayoutInput;

        FSceneGlobalData                    SceneGlobalData;

        FRHIBindingSetRef                   ToneMappingPassSet;
        FRHIBindingLayoutRef                ToneMappingPassLayout;
        
        FRHIBindingSetRef                   ShadowPassSet;
        FRHIBindingLayoutRef                ShadowPassLayout;
        
        FRHIBindingSetRef                   LightingPassSet;
        FRHIBindingLayoutRef                LightingPassLayout;

        FRHIBindingSetRef                   DebugPassSet;
        FRHIBindingLayoutRef                DebugPassLayout;

        FRHIBindingSetRef                   SSAOPassSet;
        FRHIBindingLayoutRef                SSAOPassLayout;

        FRHIBindingSetRef                   SSAOBlurPassSet;
        FRHIBindingLayoutRef                SSAOBlurPassLayout;
        
        FRHIBindingSetRef                   BindingSet;
        FRHIBindingLayoutRef                BindingLayout;

        FRHIBindingSetRef                   FrustumCullSet;
        FRHIBindingLayoutRef                FrustumCullLayout;

        FRHIBindingSetRef                   ClusterBuildSet;
        FRHIBindingLayoutRef                ClusterBuildLayout;

        FRHIBindingSetRef                   LightCullSet;
        FRHIBindingLayoutRef                LightCullLayout;

        FRHITypedVertexBuffer<FSimpleElementVertex> SimpleVertexBuffer;
        TRenderVector<FSimpleElementVertex>         SimpleVertices;
        FRHIBindingLayoutRef                        SimplePassLayout;
        
        FRHIBufferRef                               ClusterBuffer;
        FRHIBufferRef                               SceneDataBuffer;
        FRHIBufferRef                               InstanceDataBuffer;
        FRHIBufferRef                               InstanceMappingBuffer;
        FRHIBufferRef                               LightDataBuffer;
        FRHIBufferRef                               IndirectDrawBuffer;
        
        FGBuffer                            GBuffer;
        FShadowAtlas                        ShadowAtlas;

        FRHIImageRef                        HDRRenderTarget;
        FRHIImageRef                        CascadedShadowMap;
        FRHIImageRef                        PointLightShadowMap;
        FRHIImageRef                        DepthMap;
        FRHIImageRef                        DepthAttachment;
        FRHIImageRef                        NoiseImage;
        FRHIImageRef                        SSAOImage;
        FRHIImageRef                        SSAOBlur;
        FRHIImageRef                        PickerImage;
        FRHIImageRef                        DebugVisualizationImage;

        ERenderSceneDebugFlags              DebugVisualizationMode;

        TArray<FShadowCascade, NumCascades>           ShadowCascades;

        
        
        /** Packed array of per-instance data */
        TRenderVector<FInstanceData>                  InstanceData;

        /** Packed array of all cached mesh draw commands */
        TRenderVector<FMeshDrawCommand>               MeshDrawCommands;
        
        /** Packed indirect draw arguments, gets sent directly to the GPU */
        TRenderVector<FDrawIndexedIndirectArguments>  IndirectDrawArguments;
                
    };
    
}
