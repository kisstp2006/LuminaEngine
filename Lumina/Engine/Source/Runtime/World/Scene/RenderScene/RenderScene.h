#pragma once

#include "MeshDrawCommand.h"
#include "SceneRenderTypes.h"
#include "Containers/SparseArray.h"
#include "Renderer/RenderResource.h"
#include "Renderer/RenderTypes.h"
#include "Memory/Allocators/Allocator.h"
#include "world/entity/components/staticmeshcomponent.h"
#include "World/Scene/SceneInterface.h"


namespace Lumina
{
    struct FMeshBatch;
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
    
    template<typename T>
    using TRenderVector = TFixedVector<T, 1000>;
    
    class FRenderScene : public ISceneInterface
    {
    public:

        friend struct FMeshBatch;
        
        FRenderScene(CWorld* InWorld);
        virtual ~FRenderScene();
        
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

        TRenderVector<FSimpleElementVertex>     SimpleVertices;
        FRHIBindingLayoutRef                    SimplePassLayout;

        
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
        

        struct FStaticMeshRender
        {
            CMaterialInterface* Material;
            CStaticMesh* StaticMesh;
            uint64 SortKey;
            uint32 FirstIndex;
            uint32 TransformIdx;
            uint16 SurfaceIndexCount;

            auto operator <=> (const FStaticMeshRender& Other) const
            {
                return SortKey <=> Other.SortKey;
            }
        };

        struct FThreadLocalCollectionData
        {
            TRenderVector<glm::mat4> LocalTransforms;
            TRenderVector<FStaticMeshRender> LocalStaticMeshRenders;
        };

        TRenderVector<FStaticMeshRender> StaticMeshRenders;
        TFixedHashMap<uint64, FThreadLocalCollectionData, 36> ThreadResults;

        
        /** Packed array of per-instance data */
        TRenderVector<FInstanceData>                  InstanceData;

        /** Packed array of all cached mesh draw commands */
        TRenderVector<FMeshDrawCommand>               MeshDrawCommands;
        TRenderVector<FMeshDrawCommand>               DepthMeshDrawCommands;


        /** Packed indirect draw arguments, gets sent directly to the GPU */
        TRenderVector<FDrawIndexedIndirectArguments>  IndirectDrawArguments;

    };
    
}
