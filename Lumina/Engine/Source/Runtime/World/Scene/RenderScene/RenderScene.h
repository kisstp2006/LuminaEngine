#pragma once

#include "MeshDrawCommand.h"
#include "SceneRenderTypes.h"
#include "Containers/SparseArray.h"
#include "Renderer/RenderResource.h"
#include "Renderer/RenderTypes.h"
#include "Renderer/TypedBuffer.h"
#include "world/entity/components/staticmeshcomponent.h"
#include "World/Scene/SceneInterface.h"


namespace Lumina
{
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

    static constexpr int NumCascades = 4;
    
    class FRenderScene : public ISceneInterface
    {
    public:

        friend struct FMeshBatch;
        
        FRenderScene(CWorld* InWorld);
        virtual ~FRenderScene();
        
        void RenderScene(FRenderGraph& RenderGraph);
    
        FRHIImageRef GetRenderTarget() const { return SceneViewport->GetRenderTarget(); }
        LUMINA_API FRHIImageRef GetVisualizationImage() const;
        FSceneGlobalData* GetSceneGlobalData() { return &SceneGlobalData; }

        FSceneRenderSettings& GetSceneRenderSettings() { return RenderSettings; }
        
        ERenderSceneDebugFlags GetDebugMode() const { return DebugVisualizationMode; }
        void SetDebugMode(ERenderSceneDebugFlags Mode) { DebugVisualizationMode = Mode; }


        LUMINA_API entt::entity GetEntityAtPixel(uint32 X, uint32 Y) const;

    protected:

        void SetViewVolume(const FViewVolume& ViewVolume);

        
        /** Compiles all renderers from the world into draw commands for dispatch */
        void CompileDrawCommands();

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
        void DebugDrawPass(FRenderGraph& RenderGraph);
        //~ End Render Passes

        static FViewportState MakeViewportStateFromImage(const FRHIImage* Image);

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

        TRenderVector<FSimpleElementVertex>         SimpleVertices;
        FRHIBindingLayoutRef                        SimplePassLayout;
        

        FRHITypedVertexBuffer<FSimpleElementVertex> SimpleVertexBuffer;
        FRHIBufferRef                               ClusterBuffer;
        FRHIBufferRef                               SceneDataBuffer;
        FRHIBufferRef                               InstanceDataBuffer;
        FRHIBufferRef                               InstanceMappingBuffer;
        FRHIBufferRef                               InstanceReadbackBuffer;
        FRHIBufferRef                               LightDataBuffer;
        FRHIBufferRef                               IndirectDrawBuffer;

        FRHIInputLayoutRef                  VertexLayoutInput;
        FRHIInputLayoutRef                  SimpleVertexLayoutInput;

        FSceneGlobalData                    SceneGlobalData;

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
        
        FGBuffer                            GBuffer;

        FRHIImageRef                        PointLightShadowMap;
        FRHIImageRef                        ShadowAtlas;
        FRHIImageRef                        DepthMap;
        FRHIImageRef                        DepthAttachment;
        FRHIImageRef                        CubeMap;
        FRHIImageRef                        NoiseImage;
        FRHIImageRef                        SSAOImage;
        FRHIImageRef                        SSAOBlur;
        FRHIImageRef                        PickerImage;
        FRHIImageRef                        OverdrawImage;
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
