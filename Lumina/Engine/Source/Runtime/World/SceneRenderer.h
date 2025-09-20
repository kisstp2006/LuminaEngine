#pragma once

#include "SceneRenderTypes.h"
#include "Containers/SparseArray.h"
#include "Renderer/RenderResource.h"
#include "Renderer/RenderTypes.h"
#include "Entity/Components/RenderComponent.h"



namespace Lumina
{
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
    template<typename T> using TRenderVector = TFixedVector<T, 2024>;
    
    class FSceneRenderer
    {
    public:
        
        FSceneRenderer(CWorld* InWorld);
        virtual ~FSceneRenderer();
        
        void RenderScene(FRenderGraph& RenderGraph);
    
        FRHIImageRef GetRenderTarget() const { return SceneViewport->GetRenderTarget(); }
        FSceneGlobalData* GetSceneGlobalData() { return &SceneGlobalData; }

        FSceneRenderSettings& GetSceneRenderSettings() { return RenderSettings; }
        const FSceneRenderStats& GetSceneRenderStats() const { return SceneRenderStats; }
        const FGBuffer& GetGBuffer() const { return GBuffer; }
        FRHIImageRef GetDepthAttachment() const { return DepthAttachment; }
        FRHIImageRef GetSSAOImage() const { return SSAOImage; }
        
        ESceneRenderGBuffer GetGBufferDebugMode() const { return GBufferDebugMode; }
        void SetGBufferDebugMode(ESceneRenderGBuffer Mode) { GBufferDebugMode = Mode; }
        
    protected:
        
        /** Compiles all renderers from the world into draw commands for dispatch */
        void CompileDrawCommands();

        //~ Begin Render Passes
        void DepthPrePass(FRenderGraph& RenderGraph);
        void GBufferPass(FRenderGraph& RenderGraph);
        void SSAOPass(FRenderGraph& RenderGraph);
        void EnvironmentPass(FRenderGraph& RenderGraph);
        void DeferredLightingPass(FRenderGraph& RenderGraph);
        void DebugDrawPass(FRenderGraph& RenderGraph);
        //~ End Render Passes

        static FViewportState MakeViewportStateFromImage(const FRHIImage* Image);

        void InitResources();
        void InitBuffers();
        void CreateImages();
        void OnSwapchainResized();
    

    private:

        CWorld*                             World = nullptr;
        
        FSceneRenderStats                   SceneRenderStats;
        FSceneRenderSettings                RenderSettings;
        FSceneLightData                     LightData;

        FRHIViewportRef                     SceneViewport;

        TRenderVector<FSimpleElementVertex>  SimpleVertices;
        FRHIBindingLayoutRef                SimplePassLayout;

        
        FRHIBufferRef                       SimpleVertexBuffer;
        FRHIBufferRef                       SceneDataBuffer;
        FRHIBufferRef                       EnvironmentBuffer;
        FRHIBufferRef                       ModelDataBuffer;
        FRHIBufferRef                       LightDataBuffer;
        FRHIBufferRef                       SSAOKernalBuffer;
        FRHIBufferRef                       SSAOSettingsBuffer;
        FRHIBufferRef                       IndirectDrawBuffer;

        FRHIInputLayoutRef                  VertexLayoutInput;
        FRHIInputLayoutRef                  SimpleVertexLayoutInput;

        FSceneGlobalData                    SceneGlobalData;

        FRHIBindingLayoutRef                EnvironmentLayout;
        FRHIBindingSetRef                   EnvironmentBindingSet;
        FRHIBindingSetRef                   LightingPassSet;
        FRHIBindingLayoutRef                LightingPassLayout;

        FRHIBindingSetRef                   SSAOPassSet;
        FRHIBindingLayoutRef                SSAOPassLayout;

        FRHIBindingSetRef                   SSAOBlurPassSet;
        FRHIBindingLayoutRef                SSAOBlurPassLayout;
        
        FRHIBindingLayoutRef                BindingLayout;
        FRHIBindingSetRef                   BindingSet;
        
        
        FGBuffer                            GBuffer;
        
        FRHIImageRef                        DepthMap;
        FRHIImageRef                        DepthAttachment;
        FRHIImageRef                        CubeMap;
        FRHIImageRef                        IrradianceCube;
        FRHIImageRef                        ShadowCubeMap;
        FRHIImageRef                        NoiseImage;
        FRHIImageRef                        SSAOImage;
        FRHIImageRef                        SSAOBlur;

        ESceneRenderGBuffer                           GBufferDebugMode = ESceneRenderGBuffer::RenderTarget;

        TRenderVector<FInstanceData>                  InstanceData;
        
        TRenderVector<FStaticMeshRender>              StaticMeshRenders;
        TRenderVector<FIndirectRenderBatch>           RenderBatches;
        TRenderVector<FDrawIndexedIndirectArguments>  IndirectDrawArguments;

    };
    
}
