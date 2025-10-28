#include "RenderScene.h"
#include <execution>
#include <variant>
#include "Assets/AssetRegistry/AssetRegistry.h"
#include "Assets/AssetTypes/Material/Material.h"
#include "Assets/AssetTypes/Mesh/StaticMesh/StaticMesh.h"
#include "Assets/AssetTypes/Textures/Texture.h"
#include "Core/Engine/Engine.h"
#include "Core/Profiler/Profile.h"
#include "Core/Windows/Window.h"
#include "EASTL/sort.h"
#include "Renderer/RHIIncl.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtc/packing.hpp"
#include "glm/gtx/quaternion.hpp"
#include "Paths/Paths.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RHIStaticStates.h"
#include "Renderer/ShaderCompiler.h"
#include "Renderer/RenderGraph/RenderGraph.h"
#include "Renderer/RenderGraph/RenderGraphDescriptor.h"
#include "TaskSystem/TaskSystem.h"
#include "Tools/Import/ImportHelpers.h"
#include "World/World.h"
#include "World/Entity/Entity.h"
#include "World/Entity/Components/CameraComponent.h"
#include "World/Entity/Components/EditorComponent.h"
#include "world/entity/components/environmentcomponent.h"
#include "world/entity/components/lightcomponent.h"
#include "World/Entity/Components/LineBatcherComponent.h"
#include "world/entity/components/staticmeshcomponent.h"
#include "World/Entity/Components/TransformComponent.h"

    namespace Lumina
    {

        uint32 PackColor(const glm::vec3& color, float intensity)
        {
            uint32 r = static_cast<uint32>(glm::clamp(color.r, 0.0f, 1.0f) * 255.0f);
            uint32 g = static_cast<uint32>(glm::clamp(color.g, 0.0f, 1.0f) * 255.0f);
            uint32 b = static_cast<uint32>(glm::clamp(color.b, 0.0f, 1.0f) * 255.0f);
            uint32 a = static_cast<uint32>(glm::clamp(intensity, 0.0f, 1.0f) * 255.0f);
    
            return (a << 24) | (b << 16) | (g << 8) | r;
        }
        
        constexpr unsigned int ClusterGridSizeX = 16;
        constexpr unsigned int ClusterGridSizeY = 9;
        constexpr unsigned int ClusterGridSizeZ = 24;

        constexpr unsigned int NumClusters = ClusterGridSizeX * ClusterGridSizeY * ClusterGridSizeZ;

        constexpr int GShadowMapResolution      = 4096;
        constexpr int GShadowAtlasResolution    = 8192;
        constexpr int GShadowCubemapResolution  = 512;
        constexpr int GMaxPointLightShadows     = 100;

        FRenderScene::FRenderScene(CWorld* InWorld)
            : World(InWorld)
            , SceneGlobalData()
            , ShadowAtlas(FShadowAtlasConfig{8192})
        {
            DebugVisualizationMode = ERenderSceneDebugFlags::None;

            SceneViewport = GRenderContext->CreateViewport(Windowing::GetPrimaryWindowHandle()->GetExtent());

            LOG_TRACE("Initializing Renderer Scene");

            InitResources();
        }

        FRenderScene::~FRenderScene()
        {
            GRenderContext->WaitIdle();
            
            LOG_TRACE("Shutting down Renderer Scene");
        }
        
        void FRenderScene::RenderScene(FRenderGraph& RenderGraph, const FViewVolume& ViewVolume)
        {
            LUMINA_PROFILE_SCOPE();
            
            SetViewVolume(ViewVolume);

            // Wait for shader tasks.
            if(GRenderContext->GetShaderCompiler()->HasPendingRequests())
            {
                return;
            }
            
            ResetPass(RenderGraph);
            CompileDrawCommands(RenderGraph);
            CullPass(RenderGraph, SceneViewport->GetViewVolume());
            DepthPrePass(RenderGraph, SceneViewport->GetViewVolume());
            ClusterBuildPass(RenderGraph, SceneViewport->GetViewVolume());
            LightCullPass(RenderGraph, SceneViewport->GetViewVolume());
            PointShadowPass(RenderGraph);
            SpotShadowPass(RenderGraph);
            DirectionalShowPass(RenderGraph);
            GBufferPass(RenderGraph, SceneViewport->GetViewVolume());
            SSAOPass(RenderGraph);
            EnvironmentPass(RenderGraph);
            DeferredLightingPass(RenderGraph);
            BatchedLineDraw(RenderGraph);
            DebugDrawPass(RenderGraph);
        }

        FRHIImageRef FRenderScene::GetVisualizationImage() const
        {
            if (DebugVisualizationMode == ERenderSceneDebugFlags::None)
            {
                return GetRenderTarget();
            }

            return DebugVisualizationImage;
        }

        void FRenderScene::OnSwapchainResized()
        {
            CreateImages();
        }

        void FRenderScene::GrowBufferIfNeeded(FRHIBufferRef& Buffer, SIZE_T DesiredSize, SIZE_T Padding)
        {
            if (Buffer->GetDescription().Size < DesiredSize)
            {
                FRHIBufferDesc NewDesc = Buffer->GetDescription();
                NewDesc.Size = DesiredSize + Padding;

                Buffer = GRenderContext->CreateBuffer(NewDesc);
            }
        }
        

        entt::entity FRenderScene::GetEntityAtPixel(uint32 X, uint32 Y) const
        {
            if (!PickerImage)
            {
                return entt::null;
            }

            FRHICommandListRef CommandList = GRenderContext->CreateCommandList(FCommandListInfo::Graphics());
            CommandList->Open();

            FRHIStagingImageRef StagingImage = GRenderContext->CreateStagingImage(PickerImage->GetDescription(), ERHIAccess::HostRead);
            CommandList->CopyImage(PickerImage, FTextureSlice(), StagingImage, FTextureSlice());

            CommandList->Close();
            GRenderContext->ExecuteCommandList(CommandList);

            size_t RowPitch = 0;
            void* MappedMemory = GRenderContext->MapStagingTexture(StagingImage, FTextureSlice(), ERHIAccess::HostRead, &RowPitch);
            if (!MappedMemory)
            {
                return entt::null;
            }

            const uint32 Width  = PickerImage->GetDescription().Extent.X;
            const uint32 Height = PickerImage->GetDescription().Extent.Y;

            if (X >= Width || Y >= Height)
            {
                GRenderContext->UnMapStagingTexture(StagingImage);
                return entt::null;
            }

            uint8* RowStart = reinterpret_cast<uint8*>(MappedMemory) + Y * RowPitch;
            uint32* PixelPtr = reinterpret_cast<uint32*>(RowStart) + X;
            uint32 PixelValue = *PixelPtr;

            GRenderContext->UnMapStagingTexture(StagingImage);

            if (PixelValue == 0)
            {
                return entt::null;
            }

            return static_cast<entt::entity>(PixelValue);
        }

        void FRenderScene::CullPass(FRenderGraph& RenderGraph, const FViewVolume& View)
        {
            FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();

            Descriptor->AddBinding(FrustumCullSet);
            
            RenderGraph.AddPass<RG_Raster>(FRGEvent("Cull Pass"), Descriptor, [&] (ICommandList& CmdList)
            {
                LUMINA_PROFILE_SECTION_COLORED("Cull Pass", tracy::Color::Pink2);

                if (IndirectDrawArguments.empty())
                {
                    return;
                }
                
                FRHIComputeShaderRef ComputeShader = FShaderLibrary::GetComputeShader("FrustumCull.comp");

                FComputePipelineDesc PipelineDesc;
                PipelineDesc.SetComputeShader(ComputeShader);
                PipelineDesc.AddBindingLayout(FrustumCullLayout);
                    
                FRHIComputePipelineRef Pipeline = GRenderContext->CreateComputePipeline(PipelineDesc);
                
                FComputeState State;
                State.SetPipeline(Pipeline);
                State.AddBindingSet(FrustumCullSet);
                CmdList.SetComputeState(State);

                FCullData CullData;
                CullData.Frustum = View.GetFrustum();
                CullData.View = glm::vec4(View.GetViewPosition(), (uint32)InstanceData.size());
                
                CmdList.SetPushConstants(&CullData, sizeof(FCullData));

                uint32 Num = (uint32)InstanceData.size();
                uint32 NumWorkGroups = (Num + 255) / 256;
                
                CmdList.Dispatch(NumWorkGroups, 1, 1);
                
            });
        }
        
        
        void FRenderScene::DepthPrePass(FRenderGraph& RenderGraph, const FViewVolume& View)
        {
            FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();
            Descriptor->AddBinding(BindingSet);
            Descriptor->AddRawWrite(DepthAttachment);
            
            RenderGraph.AddPass<RG_Raster>(FRGEvent("Pre-Depth Pass"), Descriptor, [&] (ICommandList& CmdList)
            {
                LUMINA_PROFILE_SECTION_COLORED("Pre-Depth Pass", tracy::Color::Orange);
                
                if (MeshDrawCommands.empty())
                {
                    return;
                }
                
                FRHIVertexShaderRef VertexShader = FShaderLibrary::GetVertexShader("DepthPrePass.vert");
                if (!VertexShader)
                {
                    return;
                }

                FRenderPassDesc::FAttachment Depth; Depth
                    .SetImage(DepthAttachment)
                    .SetDepthClearValue(0.0f);
                
                FRenderPassDesc RenderPass; RenderPass
                    .SetDepthAttachment(Depth)
                    .SetRenderArea(GetRenderTarget()->GetExtent());
                
                FRenderState RenderState; RenderState
                    .SetDepthStencilState(FDepthStencilState().SetDepthFunc(EComparisonFunc::GreaterOrEqual))
                    .SetRasterState(FRasterState().EnableDepthClip());
                        
                FGraphicsPipelineDesc Desc; Desc
                    .SetRenderState(RenderState)
                    .SetInputLayout(PositionOnlyLayoutInput)
                    .AddBindingLayout(BindingLayout)
                    .SetVertexShader(VertexShader);
                    
                FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc, RenderPass);
                
                for (SIZE_T CurrentDraw = 0; CurrentDraw < MeshDrawCommands.size(); ++CurrentDraw)
                {
                    const FMeshDrawCommand& Batch = MeshDrawCommands[CurrentDraw];
                    
                    FGraphicsState GraphicsState;
                    GraphicsState.AddVertexBuffer({ Batch.VertexBuffer });
                    GraphicsState.SetIndexBuffer({ Batch.IndexBuffer });
                    GraphicsState.SetRenderPass(RenderPass);
                    GraphicsState.SetViewportState(MakeViewportStateFromImage(GetRenderTarget()));
                    GraphicsState.SetPipeline(Pipeline);
                    GraphicsState.AddBindingSet(BindingSet);
                    GraphicsState.SetIndirectParams(IndirectDrawBuffer);
                    
                    CmdList.SetGraphicsState(GraphicsState);
                    
                    CmdList.DrawIndexedIndirect(1, Batch.IndirectDrawOffset * sizeof(FDrawIndexedIndirectArguments));
                }
            });
        }

        void FRenderScene::GBufferPass(FRenderGraph& RenderGraph, const FViewVolume& View)
        {
            FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();
            Descriptor->AddBinding(BindingSet);
            Descriptor->AddRawWrite(GBuffer.Normals);
            Descriptor->AddRawWrite(GBuffer.Material);
            Descriptor->AddRawWrite(GBuffer.AlbedoSpec);
            Descriptor->AddRawWrite(PickerImage);
            Descriptor->AddRawRead(DepthAttachment);
            Descriptor->AddRawRead(IndirectDrawBuffer);
            
            RenderGraph.AddPass<RG_Raster>(FRGEvent("GBuffer Pass"), Descriptor, [&](ICommandList& CmdList)
            {
                LUMINA_PROFILE_SECTION_COLORED("GBuffer Pass", tracy::Color::Red);
                
                if (MeshDrawCommands.empty())
                {
                    return;
                }
                
                FRenderPassDesc::FAttachment Normals; Normals
                    .SetImage(GBuffer.Normals);
                
                FRenderPassDesc::FAttachment Material; Material
                    .SetImage(GBuffer.Material);
                
                FRenderPassDesc::FAttachment AlbedoSpec; AlbedoSpec
                    .SetImage(GBuffer.AlbedoSpec);
                
                FRenderPassDesc::FAttachment PickerImageAttachment; PickerImageAttachment
                    .SetImage(PickerImage);
                
                FRenderPassDesc::FAttachment Depth; Depth
                    .SetImage(DepthAttachment)
                    .SetLoadOp(ERenderLoadOp::Load);
                
                FRenderPassDesc RenderPass; RenderPass
                    .AddColorAttachment(Normals)
                    .AddColorAttachment(Material)
                    .AddColorAttachment(AlbedoSpec)
                    .AddColorAttachment(PickerImageAttachment)
                    .SetDepthAttachment(Depth)
                    .SetRenderArea(GetRenderTarget()->GetExtent());
                
                
                FRasterState RasterState;
                RasterState.EnableDepthClip();

                FDepthStencilState DepthState; DepthState
                    .SetDepthFunc(EComparisonFunc::GreaterOrEqual)
                    .DisableDepthWrite();
                
                FRenderState RenderState;
                RenderState.SetRasterState(RasterState);
                RenderState.SetDepthStencilState(DepthState);
                
                for (SIZE_T CurrentDraw = 0; CurrentDraw < MeshDrawCommands.size(); ++CurrentDraw)
                {
                    const FMeshDrawCommand& Batch = MeshDrawCommands[CurrentDraw];
                    CMaterialInterface* Mat = Batch.Material;

                    FGraphicsPipelineDesc Desc; Desc
                        .SetDebugName("GBuffer Pass")
                        .SetRenderState(RenderState)
                        .SetInputLayout(VertexLayoutInput)
                        .AddBindingLayout(BindingLayout)
                        .AddBindingLayout(Mat->GetBindingLayout())
                        .SetVertexShader(Mat->GetMaterial()->VertexShader)
                        .SetPixelShader(Mat->GetMaterial()->PixelShader);
                    
                    FGraphicsState GraphicsState; GraphicsState
                        .SetRenderPass(RenderPass)
                        .AddVertexBuffer({ Batch.VertexBuffer })
                        .SetIndexBuffer({ Batch.IndexBuffer })
                        .SetViewportState(MakeViewportStateFromImage(GetRenderTarget()))
                        .SetPipeline(GRenderContext->CreateGraphicsPipeline(Desc, RenderPass))
                        .SetIndirectParams(IndirectDrawBuffer)
                        .AddBindingSet(BindingSet)
                        .AddBindingSet(Mat->GetBindingSet());
                    
                    CmdList.SetGraphicsState(GraphicsState);
                    
                    
                    CmdList.DrawIndexedIndirect(1, Batch.IndirectDrawOffset * sizeof(FDrawIndexedIndirectArguments));
                }
            });
        }

        void FRenderScene::SSAOPass(FRenderGraph& RenderGraph)
        {
            if (RenderSettings.bSSAO)
            {
                {
                    FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();
                    Descriptor->AddBinding(BindingSet);
                    Descriptor->AddBinding(SSAOPassSet);
                    Descriptor->AddRawWrite(SSAOImage);
                    
                    RenderGraph.AddPass<RG_Raster>(FRGEvent("SSAO Pass"), Descriptor, [&] (ICommandList& CmdList)
                    {
                        LUMINA_PROFILE_SECTION_COLORED("SSAO Pass", tracy::Color::Pink);

                        FRHIVertexShaderRef VertexShader = FShaderLibrary::GetVertexShader("FullscreenQuad.vert");
                        FRHIPixelShaderRef PixelShader = FShaderLibrary::GetPixelShader("SSAO.frag");
                        if (!VertexShader || !PixelShader)
                        {
                            return;
                        }

                        FRenderPassDesc::FAttachment SSAOAttachment; SSAOAttachment
                            .SetImage(SSAOImage);

                        FRenderPassDesc RenderPass; RenderPass
                            .AddColorAttachment(SSAOAttachment)
                            .SetRenderArea(SSAOImage->GetExtent());

                        FRasterState RasterState;
                        RasterState.SetCullNone();
                        
        
                        FDepthStencilState DepthState;
                        DepthState.DisableDepthTest();
                        DepthState.DisableDepthWrite();
                
                        FRenderState RenderState;
                        RenderState.SetRasterState(RasterState);
                        RenderState.SetDepthStencilState(DepthState);
                
                        FGraphicsPipelineDesc Desc;
                        Desc.SetDebugName("SSAO Pass");
                        Desc.SetRenderState(RenderState);
                        Desc.AddBindingLayout(BindingLayout);
                        Desc.AddBindingLayout(SSAOPassLayout);
                        Desc.SetVertexShader(VertexShader);
                        Desc.SetPixelShader(PixelShader);
        
                        FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc, RenderPass);

                        FGraphicsState GraphicsState;
                        GraphicsState.SetPipeline(Pipeline);
                        GraphicsState.AddBindingSet(BindingSet);
                        GraphicsState.AddBindingSet(SSAOPassSet);
                        GraphicsState.SetRenderPass(RenderPass);
                        GraphicsState.SetViewportState(MakeViewportStateFromImage(SSAOImage));

                        CmdList.SetGraphicsState(GraphicsState);
                    
                        CmdList.Draw(3, 1, 0, 0); 
                    });
                }

                {
                    FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();
                    Descriptor->AddBinding(BindingSet);
                    Descriptor->AddBinding(SSAOBlurPassSet);
                    Descriptor->AddRawWrite(SSAOBlur);

                    RenderGraph.AddPass<RG_Raster>(FRGEvent("SSAO Blur Pass"), Descriptor, [&](ICommandList& CmdList)
                    {
                        LUMINA_PROFILE_SECTION_COLORED("SSAO Blur Pass", tracy::Color::Yellow);

                        FRHIVertexShaderRef VertexShader = FShaderLibrary::GetVertexShader("FullscreenQuad.vert");
                        FRHIPixelShaderRef PixelShader = FShaderLibrary::GetPixelShader("SSAOBlur.frag");
                        if (!VertexShader || !PixelShader)
                        {
                            return;
                        }

                        FRenderPassDesc::FAttachment SSAOAttachment; SSAOAttachment
                            .SetImage(SSAOBlur);

                        FRenderPassDesc RenderPass; RenderPass
                            .AddColorAttachment(SSAOAttachment)
                            .SetRenderArea(GetRenderTarget()->GetExtent());

                        FRasterState RasterState;
                        RasterState.SetCullNone();
        
                        FDepthStencilState DepthState;
                        DepthState.DisableDepthTest();
                        DepthState.DisableDepthWrite();
                
                        FRenderState RenderState;
                        RenderState.SetRasterState(RasterState);
                        RenderState.SetDepthStencilState(DepthState);
                
                        FGraphicsPipelineDesc Desc;
                        Desc.SetDebugName("SSAO Blur Pass");
                        Desc.SetRenderState(RenderState);
                        Desc.AddBindingLayout(BindingLayout);
                        Desc.AddBindingLayout(SSAOBlurPassLayout);
                        Desc.SetVertexShader(VertexShader);
                        Desc.SetPixelShader(PixelShader);
        
                        FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc, RenderPass);

                        FGraphicsState GraphicsState;
                        GraphicsState.SetPipeline(Pipeline);
                        GraphicsState.AddBindingSet(BindingSet);
                        GraphicsState.AddBindingSet(SSAOBlurPassSet);
                        GraphicsState.SetRenderPass(RenderPass);
                        GraphicsState.SetViewportState(MakeViewportStateFromImage(GetRenderTarget()));

                        CmdList.SetGraphicsState(GraphicsState);
                    
                        CmdList.Draw(3, 1, 0, 0); 
                    });
                }
            }
            else
            {
                FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();
                Descriptor->AddRawWrite(SSAOBlur);
                
                RenderGraph.AddPass<RG_Raster>(FRGEvent("SSAO Clear Pass"), Descriptor, [&](ICommandList& CmdList)
                {
                    CmdList.ClearImageUInt(SSAOBlur, AllSubresources, 0.0f);
                });
            }
        }

        void FRenderScene::EnvironmentPass(FRenderGraph& RenderGraph)
        {
            if (RenderSettings.bHasEnvironment)
            {
                FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();
                Descriptor->AddBinding(BindingSet);
                Descriptor->AddRawWrite(GetRenderTarget());

                RenderGraph.AddPass<RG_Raster>(FRGEvent("Environment Pass"), Descriptor, [&](ICommandList& CmdList)
                {
                    LUMINA_PROFILE_SECTION_COLORED("Environment Pass", tracy::Color::Green3);
            
                    FRHIVertexShaderRef VertexShader = FShaderLibrary::GetVertexShader("FullscreenQuad.vert");
                    FRHIPixelShaderRef PixelShader = FShaderLibrary::GetPixelShader("Environment.frag");
                    if (!VertexShader || !PixelShader)
                    {
                        return;
                    }

                    FRenderPassDesc::FAttachment Attachment; Attachment
                        .SetImage(GetRenderTarget());


                    FRenderPassDesc RenderPass; RenderPass
                        .AddColorAttachment(Attachment)
                        .SetRenderArea(GetRenderTarget()->GetExtent());
            
                    FRasterState RasterState;
                    RasterState.SetCullNone();
                    
                    FRenderState RenderState;
                    RenderState.SetRasterState(RasterState);
            
                    FGraphicsPipelineDesc Desc;
                    Desc.SetDebugName("Environment Pass");
                    Desc.SetRenderState(RenderState);
                    Desc.AddBindingLayout(BindingLayout);
                    Desc.SetVertexShader(VertexShader);
                    Desc.SetPixelShader(PixelShader);
            
                    FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc, RenderPass);
            
                    FGraphicsState GraphicsState;
                    GraphicsState.AddBindingSet(BindingSet);
                    GraphicsState.SetPipeline(Pipeline);
                    GraphicsState.SetRenderPass(RenderPass);
                    GraphicsState.SetViewportState(MakeViewportStateFromImage(GetRenderTarget()));
          
                    CmdList.SetGraphicsState(GraphicsState);
                
                    CmdList.Draw(3, 1, 0, 0); 
                });
            }
        }

        void FRenderScene::DeferredLightingPass(FRenderGraph& RenderGraph)
        {
            FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();
            Descriptor->AddBinding(BindingSet);
            Descriptor->AddBinding(LightingPassSet);
            Descriptor->AddRawWrite(GetRenderTarget());
            
            RenderGraph.AddPass<RG_Raster>(FRGEvent("Deferred Lighting Pass"), Descriptor, [&](ICommandList& CmdList)
            {
                LUMINA_PROFILE_SECTION_COLORED("Deferred Lighting Pass", tracy::Color::Red2);
                
                FRHIVertexShaderRef VertexShader = FShaderLibrary::GetVertexShader("FullscreenQuad.vert");
                FRHIPixelShaderRef PixelShader = FShaderLibrary::GetPixelShader("DeferredLighting.frag");
                if (!VertexShader || !PixelShader)
                {
                    return;
                }

                FRenderPassDesc::FAttachment Attachment; Attachment
                    .SetImage(GetRenderTarget());

                if (RenderSettings.bHasEnvironment)
                {
					Attachment.SetLoadOp(ERenderLoadOp::Load);
                }


                FRenderPassDesc RenderPass; RenderPass
                    .AddColorAttachment(Attachment)
                    .SetRenderArea(GetRenderTarget()->GetExtent());
                
                FRasterState RasterState;
                RasterState.SetCullNone();

                FRenderState RenderState;
                RenderState.SetRasterState(RasterState);
                
                FGraphicsPipelineDesc Desc;
                Desc.SetDebugName("Lighting Pass");
                Desc.SetRenderState(RenderState);
                Desc.AddBindingLayout(BindingLayout);
                Desc.AddBindingLayout(LightingPassLayout);
                Desc.SetVertexShader(VertexShader);
                Desc.SetPixelShader(PixelShader);
        
                FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc, RenderPass);

                FGraphicsState GraphicsState;
                GraphicsState.SetPipeline(Pipeline);
                GraphicsState.AddBindingSet(BindingSet);
                GraphicsState.AddBindingSet(LightingPassSet);
                GraphicsState.SetRenderPass(RenderPass);                
                GraphicsState.SetViewportState(MakeViewportStateFromImage(GetRenderTarget()));

                CmdList.SetGraphicsState(GraphicsState);
                
                CmdList.Draw(3, 1, 0, 0); 
            });
        }

        void FRenderScene::BatchedLineDraw(FRenderGraph& RenderGraph)
        {
            FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();
            Descriptor->AddBinding(BindingSet);
            Descriptor->AddRawWrite(GetRenderTarget());
            Descriptor->AddRawRead(DepthAttachment);
            
            RenderGraph.AddPass<RG_Raster>(FRGEvent("Batched Line Pass"), Descriptor, [&](ICommandList& CmdList)
            {
                LUMINA_PROFILE_SECTION_COLORED("Batched Line Pass", tracy::Color::Yellow3);
                
                FRHIVertexShaderRef VertexShader = FShaderLibrary::GetVertexShader("SimpleElement.vert");
                FRHIPixelShaderRef PixelShader = FShaderLibrary::GetPixelShader("SimpleElement.frag");
                if (!VertexShader || !PixelShader || SimpleVertices.empty())
                {
                    return;
                }


                FRenderPassDesc::FAttachment Attachment; Attachment
                    .SetLoadOp(ERenderLoadOp::Load)
                    .SetImage(GetRenderTarget());

                FRenderPassDesc::FAttachment Depth; Depth
                    .SetImage(DepthAttachment)
                    .SetLoadOp(ERenderLoadOp::Load);

                FRenderPassDesc RenderPass; RenderPass
                    .AddColorAttachment(Attachment)
                    .SetDepthAttachment(Depth)
                    .SetRenderArea(GetRenderTarget()->GetExtent());

                
                FRasterState RasterState;
                RasterState.SetCullNone();
                RasterState.SetLineWidth(2.5f);
        
                FDepthStencilState DepthState;
                DepthState.EnableDepthTest();
                DepthState.SetDepthFunc(EComparisonFunc::GreaterOrEqual);
                DepthState.DisableDepthWrite();
                
                FRenderState RenderState;
                RenderState.SetRasterState(RasterState);
                RenderState.SetDepthStencilState(DepthState);
                
                FGraphicsPipelineDesc Desc;
                Desc.SetDebugName("Batched Line Draw");
                Desc.SetPrimType(EPrimitiveType::LineList);
                Desc.SetInputLayout(SimpleVertexLayoutInput);
                Desc.SetRenderState(RenderState);
                Desc.AddBindingLayout(SimplePassLayout);
                Desc.SetVertexShader(VertexShader);
                Desc.SetPixelShader(PixelShader);
        
                FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc, RenderPass);

                FGraphicsState GraphicsState;
                GraphicsState.SetPipeline(Pipeline);
                GraphicsState.AddVertexBuffer({ SimpleVertexBuffer });
                GraphicsState.SetRenderPass(RenderPass);
                GraphicsState.SetViewportState(MakeViewportStateFromImage(GetRenderTarget()));

                CmdList.SetGraphicsState(GraphicsState);

                SCameraComponent& CameraComponent = World->GetActiveCamera();
                glm::mat4 ViewProjMatrix = CameraComponent.GetViewProjectionMatrix();
                CmdList.SetPushConstants(&ViewProjMatrix, sizeof(glm::mat4));
                CmdList.Draw((uint32)SimpleVertices.size(), 1, 0, 0); 
            });
        }

        void FRenderScene::DebugDrawPass(FRenderGraph& RenderGraph)
        {
            FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();
            Descriptor->AddBinding(BindingSet);
            Descriptor->AddBinding(DebugPassSet);
            Descriptor->AddRawWrite(DebugVisualizationImage);

            
            RenderGraph.AddPass<RG_Raster>(FRGEvent("Debug Draw Pass"), Descriptor, [&](ICommandList& CmdList)
            {
                LUMINA_PROFILE_SECTION_COLORED("Debug Draw Pass", tracy::Color::Red2);
                
                FRHIVertexShaderRef VertexShader = FShaderLibrary::GetVertexShader("FullscreenQuad.vert");
                FRHIPixelShaderRef PixelShader = FShaderLibrary::GetPixelShader("Debug.frag");
                if (!VertexShader || !PixelShader)
                {
                    return;
                }
                
                FRenderPassDesc::FAttachment Attachment; Attachment
                    .SetImage(DebugVisualizationImage);

                FRenderPassDesc RenderPass; RenderPass
                    .AddColorAttachment(Attachment)
                    .SetRenderArea(DebugVisualizationImage->GetExtent());


                FRasterState RasterState;
                RasterState.SetCullNone();
                
                FDepthStencilState DepthState;
                DepthState.DisableDepthTest();
                DepthState.DisableDepthWrite();
                
                FRenderState RenderState;
                RenderState.SetRasterState(RasterState);
                RenderState.SetDepthStencilState(DepthState);
                
                FGraphicsPipelineDesc Desc;
                Desc.SetDebugName("Debug Draw Pass");
                Desc.SetRenderState(RenderState);
                Desc.AddBindingLayout(BindingLayout);
                Desc.AddBindingLayout(DebugPassLayout);
                Desc.SetVertexShader(VertexShader);
                Desc.SetPixelShader(PixelShader);
        
                FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc, RenderPass);

                FGraphicsState GraphicsState;
                GraphicsState.SetPipeline(Pipeline);
                GraphicsState.AddBindingSet(BindingSet);
                GraphicsState.AddBindingSet(DebugPassSet);
                GraphicsState.SetRenderPass(RenderPass);               
                GraphicsState.SetViewportState(MakeViewportStateFromImage(DebugVisualizationImage));

                CmdList.SetGraphicsState(GraphicsState);

                uint32 Mode = (uint32)DebugVisualizationMode;
                CmdList.SetPushConstants(&Mode, sizeof(uint32));
                CmdList.Draw(3, 1, 0, 0); 
            });
        }

        FViewportState FRenderScene::MakeViewportStateFromImage(const FRHIImage* Image)
        {
            float SizeY = (float)Image->GetSizeY();
            float SizeX = (float)Image->GetSizeX();

            FViewportState ViewportState;
            ViewportState.Viewports.emplace_back(FViewport(SizeX, SizeY));
            ViewportState.Scissors.emplace_back(FRect(SizeX, SizeY));

            return ViewportState;
        }

        void FRenderScene::CheckInstanceBufferResize(uint32 NumInstances)
        {
            uint32 SizeRequiredBytes = NumInstances * sizeof(FInstanceData);
            uint32 NumToReallocate = NumInstances * 2;
            
            if (InstanceDataBuffer->GetDescription().Size < SizeRequiredBytes)
            {
                {
                    FRHIBufferDesc BufferDesc;
                    BufferDesc.Size = sizeof(FInstanceData) * NumToReallocate;
                    BufferDesc.Usage.SetMultipleFlags(BUF_StorageBuffer);
                    BufferDesc.bKeepInitialState = true;
                    BufferDesc.InitialState = EResourceStates::ShaderResource;
                    BufferDesc.DebugName = "Instance Buffer";
                    InstanceDataBuffer = GRenderContext->CreateBuffer(BufferDesc);
                }

            
                {
                    FRHIBufferDesc BufferDesc;
                    BufferDesc.Size = sizeof(uint32) * NumToReallocate;
                    BufferDesc.Usage.SetMultipleFlags(BUF_StorageBuffer);
                    BufferDesc.bKeepInitialState = true;
                    BufferDesc.InitialState = EResourceStates::UnorderedAccess;
                    BufferDesc.DebugName = "Instance Mapping";
                    InstanceMappingBuffer = GRenderContext->CreateBuffer(BufferDesc);
                }


                {
                    FBindingSetDesc BindingSetDesc;
                    BindingSetDesc.AddItem(FBindingSetItem::BufferSRV(0, InstanceDataBuffer));
                    BindingSetDesc.AddItem(FBindingSetItem::BufferUAV(1, InstanceMappingBuffer));
                    BindingSetDesc.AddItem(FBindingSetItem::BufferUAV(2, IndirectDrawBuffer));
                    BindingSetDesc.AddItem(FBindingSetItem::PushConstants(0, sizeof(FCullData)));

                    TBitFlags<ERHIShaderType> Visibility;
                    Visibility.SetMultipleFlags(ERHIShaderType::Compute);
                    GRenderContext->CreateBindingSetAndLayout(Visibility, 0, BindingSetDesc, FrustumCullLayout, FrustumCullSet);
                }
            
                {

                    FBindingSetDesc BindingSetDesc;
                    BindingSetDesc.AddItem(FBindingSetItem::BufferCBV(0, SceneDataBuffer));
                    BindingSetDesc.AddItem(FBindingSetItem::BufferSRV(1, InstanceDataBuffer));
                    BindingSetDesc.AddItem(FBindingSetItem::BufferSRV(2, InstanceMappingBuffer));
                    BindingSetDesc.AddItem(FBindingSetItem::BufferSRV(3, LightDataBuffer));

                    TBitFlags<ERHIShaderType> Visibility;
                    Visibility.SetMultipleFlags(ERHIShaderType::Vertex, ERHIShaderType::Fragment);
                    GRenderContext->CreateBindingSetAndLayout(Visibility, 0, BindingSetDesc, BindingLayout, BindingSet);
                }
            }
        }

        void FRenderScene::SetViewVolume(const FViewVolume& ViewVolume)
        {
            SceneViewport->SetViewVolume(ViewVolume);
            
            SceneGlobalData.CameraData.Location             = glm::vec4(SceneViewport->GetViewVolume().GetViewPosition(), 1.0f);
            SceneGlobalData.CameraData.View                 = SceneViewport->GetViewVolume().GetViewMatrix();
            SceneGlobalData.CameraData.InverseView          = SceneViewport->GetViewVolume().GetInverseViewMatrix();
            SceneGlobalData.CameraData.Projection           = SceneViewport->GetViewVolume().GetProjectionMatrix();
            SceneGlobalData.CameraData.InverseProjection    = SceneViewport->GetViewVolume().GetInverseProjectionMatrix();
            SceneGlobalData.ScreenSize                      = glm::vec4(SceneViewport->GetSize().X, SceneViewport->GetSize().Y, 0.0f, 0.0f);
            SceneGlobalData.GridSize                        = glm::vec4(ClusterGridSizeX, ClusterGridSizeY, ClusterGridSizeZ, 0.0f);
            SceneGlobalData.Time                            = (float)World->GetTimeSinceWorldCreation();
            SceneGlobalData.DeltaTime                       = (float)World->GetWorldDeltaTime();
            SceneGlobalData.FarPlane                        = SceneViewport->GetViewVolume().GetFar();
            SceneGlobalData.NearPlane                       = SceneViewport->GetViewVolume().GetNear();

        }

        void FRenderScene::ResetPass(FRenderGraph& RenderGraph)
        {
            SimpleVertices.clear();
            MeshDrawCommands.clear();
            IndirectDrawArguments.clear();
            InstanceData.clear();
            LightData.NumLights = 0;
            ShadowAtlas.FreeTiles();

            FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();
            Descriptor->AddRawWrite(OverdrawImage);
            
            RenderGraph.AddPass<RG_Raster>(FRGEvent("Clear Overdraw"), Descriptor, [&](ICommandList& CmdList)
            {
               CmdList.ClearImageUInt(OverdrawImage, AllSubresources, 0); 
            });
        }

        void FRenderScene::ClusterBuildPass(FRenderGraph& RenderGraph, const FViewVolume& View)
        {
            FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();
            Descriptor->AddBinding(ClusterBuildSet);
            
            RenderGraph.AddPass<RG_Raster>(FRGEvent("Cluster Build Pass"), Descriptor, [&] (ICommandList& CmdList)
            {
                LUMINA_PROFILE_SECTION_COLORED("Cluster Build Pass", tracy::Color::Pink2);
                
                FRHIComputeShaderRef ComputeShader = FShaderLibrary::GetComputeShader("ClusterBuild.comp");

                FComputePipelineDesc PipelineDesc;
                PipelineDesc.SetComputeShader(ComputeShader);
                PipelineDesc.AddBindingLayout(ClusterBuildLayout);
                    
                FRHIComputePipelineRef Pipeline = GRenderContext->CreateComputePipeline(PipelineDesc);
                
                FComputeState State;
                State.SetPipeline(Pipeline);
                State.AddBindingSet(ClusterBuildSet);
                CmdList.SetComputeState(State);

                FLightClusterPC ClusterPC;
                ClusterPC.InverseProjection = View.GetInverseProjectionMatrix();
                ClusterPC.zNearFar = glm::vec2(View.GetNear(), View.GetFar());
                ClusterPC.GridSize = glm::vec4(ClusterGridSizeX, ClusterGridSizeY, ClusterGridSizeZ, 0.0f);
                ClusterPC.ScreenSize = glm::vec2(GetRenderTarget()->GetSizeX(), GetRenderTarget()->GetSizeY());
                
                CmdList.SetPushConstants(&ClusterPC, sizeof(FLightClusterPC));
                
                CmdList.Dispatch(ClusterGridSizeX, ClusterGridSizeY, ClusterGridSizeZ);
                
            });
        }
        

        void FRenderScene::LightCullPass(FRenderGraph& RenderGraph, const FViewVolume& View)
        {
            FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();
            Descriptor->AddBinding(LightCullSet);
            
            RenderGraph.AddPass<RG_Raster>(FRGEvent("Light Cull Pass"), Descriptor, [&] (ICommandList& CmdList)
            {
                LUMINA_PROFILE_SECTION_COLORED("Light Cull Pass", tracy::Color::Pink2);
                
                FRHIComputeShaderRef ComputeShader = FShaderLibrary::GetComputeShader("LightCull.comp");

                FComputePipelineDesc PipelineDesc;
                PipelineDesc.SetComputeShader(ComputeShader);
                PipelineDesc.AddBindingLayout(LightCullLayout);
                    
                FRHIComputePipelineRef Pipeline = GRenderContext->CreateComputePipeline(PipelineDesc);
                
                FComputeState State;
                State.SetPipeline(Pipeline);
                State.AddBindingSet(LightCullSet);
                CmdList.SetComputeState(State);
                
                glm::mat4 ViewProj = View.GetViewMatrix();
                
                CmdList.SetPushConstants(&ViewProj, sizeof(glm::mat4));
                
                CmdList.Dispatch(27, 1, 1);
                
            });
        }

        void FRenderScene::PointShadowPass(FRenderGraph& RenderGraph)
        {
            FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();
            Descriptor->AddBinding(BindingSet);
            Descriptor->AddBinding(ShadowPassSet);
            Descriptor->AddRawWrite(PointLightShadowMap);
            Descriptor->AddRawRead(IndirectDrawBuffer);

            RenderGraph.AddPass<RG_Raster>(FRGEvent("Point Light Shadow Pass"), Descriptor, [&](ICommandList& CmdList)
            {
                LUMINA_PROFILE_SECTION_COLORED("Point Light Shadow Pass", tracy::Color::DeepPink2);

                if (LightData.NumLights == 0 )
                {
                    return;
                }

                FRHIVertexShaderRef VertexShader = FShaderLibrary::GetVertexShader("ShadowMapping.vert");
                FRHIPixelShaderRef PixelShader = FShaderLibrary::GetPixelShader("ShadowMapping.frag");
                
                FRenderState RenderState; RenderState
                    .SetDepthStencilState(FDepthStencilState()
                        .SetDepthFunc(EComparisonFunc::LessOrEqual))
                        .SetRasterState(FRasterState().SetSlopeScaleDepthBias(1.75f).SetDepthBias(100).SetCullFront());
                
                FGraphicsPipelineDesc Desc; Desc
                    .SetDebugName("Point Light Shadow Pass")
                    .SetRenderState(RenderState)
                    .SetInputLayout(PositionOnlyLayoutInput)
                    .AddBindingLayout(BindingLayout)
                    .AddBindingLayout(ShadowPassLayout)
                    .SetVertexShader(VertexShader)
                    .SetPixelShader(PixelShader);

                
                for (size_t LightNum = 0; LightNum < LightData.NumLights; ++LightNum)
                {
                    const FLight& Light = LightData.Lights[LightNum];
                    
                    if (Light.Flags != LIGHT_TYPE_POINT || Light.Shadow.ShadowMapIndex == INDEX_NONE)
                    {
                        continue;
                    }
                    
                    float SizeY = (float)GShadowCubemapResolution;
                    float SizeX = (float)GShadowCubemapResolution;
            
                    FViewportState ViewportState;
                    for (int i = 0; i < 6; ++i)
                    {
                        ViewportState.Viewports.emplace_back(FViewport(SizeX, SizeY));
                        ViewportState.Scissors.emplace_back(FRect(SizeX, SizeY));
                    }
                        
            
                    uint32 BaseLayerIndex = Light.Shadow.ShadowMapIndex * 6;
                        
                    FRenderPassDesc::FAttachment Depth; Depth
                        .SetImage(PointLightShadowMap)
                        .SetArraySliceRange(BaseLayerIndex, 6)
                        .SetDepthClearValue(1.0f);
                
                    FRenderPassDesc RenderPass; RenderPass
                        .SetDepthAttachment(Depth)
                        .SetRenderArea(FIntVector2D(GShadowCubemapResolution, GShadowCubemapResolution));

                    FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc, RenderPass);
                    
                    for (SIZE_T CurrentDraw = 0; CurrentDraw < MeshDrawCommands.size(); ++CurrentDraw)
                    {
                        const FMeshDrawCommand& Batch = MeshDrawCommands[CurrentDraw];

                        FGraphicsState GraphicsState; GraphicsState
                            .SetRenderPass(RenderPass)
                            .SetViewportState(ViewportState)
                            .SetPipeline(Pipeline)
                            .AddBindingSet(BindingSet)
                            .AddBindingSet(ShadowPassSet)
                            .SetIndirectParams(IndirectDrawBuffer);
                        
                        GraphicsState.AddVertexBuffer({ Batch.VertexBuffer }).SetIndexBuffer({ Batch.IndexBuffer });
                        CmdList.SetGraphicsState(GraphicsState);
                        
                        CmdList.SetPushConstants(&LightNum, sizeof(uint32));
                        CmdList.DrawIndexedIndirect(1, Batch.IndirectDrawOffset * sizeof(FDrawIndexedIndirectArguments));
                    }
                }
            });
        }

        void FRenderScene::SpotShadowPass(FRenderGraph& RenderGraph)
        {
            FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();
            Descriptor->AddBinding(BindingSet);
            Descriptor->AddBinding(ShadowPassSet);
            Descriptor->AddRawWrite(ShadowAtlas.GetImage());
            Descriptor->AddRawRead(IndirectDrawBuffer);

            
            RenderGraph.AddPass<RG_Raster>(FRGEvent("Spot Shadow Pass"), Descriptor, [&](ICommandList& CmdList)
            {
                LUMINA_PROFILE_SECTION_COLORED("Spot Shadow Pass", tracy::Color::DeepPink4);
                
                FRHIVertexShaderRef VertexShader = FShaderLibrary::GetVertexShader("ShadowMapping.vert");
                FRHIPixelShaderRef PixelShader = FShaderLibrary::GetPixelShader("ShadowMapping.frag");
                
                FRenderState RenderState; RenderState
                    .SetDepthStencilState(FDepthStencilState()
                        .SetDepthFunc(EComparisonFunc::LessOrEqual))
                        .SetRasterState(FRasterState().SetSlopeScaleDepthBias(1.75f).SetDepthBias(100).SetCullFront());

                
                FGraphicsPipelineDesc Desc; Desc
                    .SetDebugName("Spot Shadow Pass")
                    .SetRenderState(RenderState)
                    .SetInputLayout(PositionOnlyLayoutInput)
                    .AddBindingLayout(BindingLayout)
                    .AddBindingLayout(ShadowPassLayout)
                    .SetVertexShader(VertexShader)
                    .SetPixelShader(PixelShader);
                    
                
                
                for (size_t LightNum = 0; LightNum < LightData.NumLights; ++LightNum)
                {
                    FLight& Light = LightData.Lights[LightNum];
                    
                    if (Light.Flags != LIGHT_TYPE_SPOT || Light.Shadow.ShadowMapIndex == INDEX_NONE)
                    {
                        continue;
                    }

                    const FShadowTile& Tile = ShadowAtlas.GetTile(Light.Shadow.ShadowMapIndex);
                    uint32 TilePixelX = static_cast<uint32>(Tile.UVOffset.x * GShadowAtlasResolution);
                    uint32 TilePixelY = static_cast<uint32>(Tile.UVOffset.y * GShadowAtlasResolution);
                    uint32 TileSize = static_cast<uint32>(Tile.UVScale.x * GShadowAtlasResolution);
                    
                    FRenderPassDesc::FAttachment Depth; Depth
                        .SetImage(ShadowAtlas.GetImage())
                        .SetDepthClearValue(1.0f);
                    
                    FRenderPassDesc RenderPass; RenderPass
                        .SetDepthAttachment(Depth)
                        .SetRenderArea(FIntVector2D(GShadowAtlasResolution, GShadowAtlasResolution));

                    FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc, RenderPass);

                    
                    FViewportState ViewportState;
                    ViewportState.Viewports.emplace_back(FViewport
                    (
                        (float)TilePixelX,
                        (float)TilePixelX + TileSize,
                        (float)TilePixelY,
                        (float)TilePixelY + TileSize,
                        0.0f,
                        1.0f 
                    ));

                    // FRect(minX, maxX, minY, maxY)
                    ViewportState.Scissors.emplace_back(FRect
                    (
                        (int)TilePixelX,
                        (int)TilePixelX + TileSize,
                        (int)TilePixelY,
                        (int)TilePixelY + TileSize
                    ));
                    
                    FGraphicsState GraphicsState; GraphicsState
                        .SetRenderPass(RenderPass)
                        .SetViewportState(ViewportState)
                        .SetPipeline(Pipeline)
                        .AddBindingSet(BindingSet)
                        .AddBindingSet(ShadowPassSet)
                        .SetIndirectParams(IndirectDrawBuffer);                    


                    
                    for (SIZE_T CurrentDraw = 0; CurrentDraw < MeshDrawCommands.size(); ++CurrentDraw)
                    {
                        const FMeshDrawCommand& Batch = MeshDrawCommands[CurrentDraw];

                        GraphicsState.SetVertexBuffer({Batch.VertexBuffer});
                        GraphicsState.SetIndexBuffer({Batch.IndexBuffer});
                        CmdList.SetGraphicsState(GraphicsState);
                        
                        CmdList.SetPushConstants(&LightNum, sizeof(uint32));
                        CmdList.DrawIndexedIndirect(1, Batch.IndirectDrawOffset * sizeof(FDrawIndexedIndirectArguments));
                    }
                }
            });
        }

        void FRenderScene::DirectionalShowPass(FRenderGraph& RenderGraph)
        {
            FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();
            Descriptor->AddBinding(BindingSet);
            Descriptor->AddBinding(ShadowPassSet);
            Descriptor->AddRawRead(IndirectDrawBuffer);
            
            
            RenderGraph.AddPass<RG_Raster>(FRGEvent("Cascaded Shadow Map Pass"), Descriptor, [&](ICommandList& CmdList)
            {
                LUMINA_PROFILE_SECTION_COLORED("Cascaded Shadow Map Pass", tracy::Color::DeepPink2);

                if (!LightData.bHasSun)
                {
                    return;
                }

                FRHIVertexShaderRef VertexShader = FShaderLibrary::GetVertexShader("ShadowMapping.vert");
                //FRHIPixelShaderRef PixelShader = FShaderLibrary::GetPixelShader("ShadowMapping.frag");

                FRenderState RenderState; RenderState
                    .SetDepthStencilState(FDepthStencilState()
                        .SetDepthFunc(EComparisonFunc::LessOrEqual))
                        .SetRasterState(FRasterState().SetSlopeScaleDepthBias(1.75f).SetDepthBias(100).SetCullFront());
                            
                FGraphicsPipelineDesc Desc; Desc
                    .SetDebugName("Cascaded Shadow Maps")
                    .SetRenderState(RenderState)
                    .SetInputLayout(PositionOnlyLayoutInput)
                    .AddBindingLayout(BindingLayout)
                    .AddBindingLayout(ShadowPassLayout)
                    .SetVertexShader(VertexShader);
                    //.SetPixelShader(PixelShader);

                
                for (int i = 0; i < NumCascades; ++i)
                {
                    FShadowCascade& Cascade = ShadowCascades[i];

                    FRenderPassDesc::FAttachment Depth; Depth
                        .SetImage(CascadedShadowMap)
                        .SetDepthClearValue(1.0f);
                
                    FRenderPassDesc RenderPass; RenderPass
                        .SetDepthAttachment(Depth, FTextureSubresourceSet(0, 1, i, 1))
                        .SetRenderArea(FIntVector2D(Cascade.ShadowMapSize.x, Cascade.ShadowMapSize.y));

                    FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc, RenderPass);
                    
                    for (SIZE_T CurrentDraw = 0; CurrentDraw < MeshDrawCommands.size(); ++CurrentDraw)
                    {
                        const FMeshDrawCommand& Batch = MeshDrawCommands[CurrentDraw];

                        FGraphicsState GraphicsState; GraphicsState
                            .SetRenderPass(RenderPass)
                            .SetViewportState(MakeViewportStateFromImage(CascadedShadowMap))
                            .SetPipeline(Pipeline)
                            .AddBindingSet(BindingSet)
                            .AddBindingSet(ShadowPassSet)
                            .SetIndirectParams(IndirectDrawBuffer)
                            .AddVertexBuffer({Batch.VertexBuffer})
                            .SetIndexBuffer({Batch.IndexBuffer});
                        
                        CmdList.SetGraphicsState(GraphicsState);

                        uint32 LightIndex = 0;
                        CmdList.SetPushConstants(&LightIndex, sizeof(uint32));
                        CmdList.DrawIndexedIndirect(1, Batch.IndirectDrawOffset * sizeof(FDrawIndexedIndirectArguments));
                    }
                }
            });
        }

        void FRenderScene::CompileDrawCommands(FRenderGraph& RenderGraph)
        {
            LUMINA_PROFILE_SCOPE();

            {
                LUMINA_PROFILE_SECTION("Compile Draw Commands");
                
                auto Group = World->GetEntityRegistry().group<SStaticMeshComponent, STransformComponent>();

                const size_t EntityCount = Group.size();
                const size_t EstimatedProxies = EntityCount * 2;
                
                InstanceData.clear();
                InstanceData.reserve(EstimatedProxies);
                
                THashMap<uint64, uint64> BatchedDraws;
                
                //========================================================================================================================
                {
                    Group.each([&](entt::entity entity, const SStaticMeshComponent& MeshComponent, const STransformComponent& TransformComponent)
                    {
                        CStaticMesh* Mesh = MeshComponent.StaticMesh;
                        if (!IsValid(Mesh))
                        {
                            return;
                        }

                        const FMeshResource& Resource = Mesh->GetMeshResource();
                        const uintptr_t MeshPtr = reinterpret_cast<uintptr_t>(Mesh);
                        const uint64 MeshID = (MeshPtr & 0xFFFFFull) << 24;
                        
                        if (MeshComponent.Instances.empty())
                        {
                            glm::mat4 TransformMatrix = TransformComponent.GetMatrix();
                            
                            FAABB BoundingBox       = Mesh->GetAABB().ToWorld(TransformMatrix);
                            glm::vec3 Center        = (BoundingBox.Min + BoundingBox.Max) * 0.5f;
                            glm::vec3 Extents       = BoundingBox.Max - Center;
                            float Radius            = glm::length(Extents);
                            glm::vec4 SphereBounds  = glm::vec4(Center, Radius);
                        
                            for (const FGeometrySurface& Surface : Resource.GeometrySurfaces)
                            {
                                CMaterialInterface* Material = MeshComponent.GetMaterialForSlot(Surface.MaterialIndex);
                            
                                if (!IsValid(Material) || !Material->IsReadyForRender())
                                {
                                    Material = CMaterial::GetDefaultMaterial();
                                }
                            
                                const uintptr_t MaterialPtr = reinterpret_cast<uintptr_t>(Material);
                                const uint64 MaterialID = (MaterialPtr & 0xFFFFFFFull) << 28;
                                uint64 SortKey = MaterialID | MeshID;
                                if (RenderSettings.bUseInstancing == false)
                                {
                                    SortKey = (uint64)entity;
                                }
                                
                                if (BatchedDraws.find(SortKey) == BatchedDraws.end())
                                {
                                    BatchedDraws[SortKey] = IndirectDrawArguments.size();

                                    MeshDrawCommands.emplace_back(FMeshDrawCommand
                                    {
                                        .Material = Material,
                                        .IndexBuffer =  Mesh->GetIndexBuffer(),
                                        .VertexBuffer = Mesh->GetVertexBuffer(),
                                        .IndirectDrawOffset = (uint32)IndirectDrawArguments.size()
                                    });
                                
                                    IndirectDrawArguments.emplace_back(FDrawIndexedIndirectArguments
                                    {
                                        .IndexCount = Surface.IndexCount,
                                        .InstanceCount = 1,
                                        .StartIndexLocation = Surface.StartIndex,
                                        .BaseVertexLocation = 0,
                                        .StartInstanceLocation = 0,
                                    });
                                }
                                else
                                {
                                    IndirectDrawArguments[BatchedDraws[SortKey]].InstanceCount++;
                                }

                                glm::uvec4 PackedID;
                                PackedID.x = (uint32)entity;
                                PackedID.y = (uint32)BatchedDraws[SortKey]; // Get index of indirect draw batch.
                                PackedID.z = 0;
                                if (entity == World->GetSelectedEntity())
                                {
                                    PackedID.z = true;
                                }
                            
                                InstanceData.emplace_back(FInstanceData
                                {
                                    .Transform = TransformMatrix,
                                    .SphereBounds = SphereBounds,
                                    .PackedID = PackedID,
                                });   
                            }
                        }
                        else
                        {
                            for (const FGeometrySurface& Surface : Resource.GeometrySurfaces)
                            {
                                CMaterialInterface* Material = MeshComponent.GetMaterialForSlot(Surface.MaterialIndex);

                                if (!IsValid(Material) || !Material->IsReadyForRender())
                                {
                                    Material = CMaterial::GetDefaultMaterial();
                                }
                                
                                const uintptr_t MaterialPtr = reinterpret_cast<uintptr_t>(Material);
                                const uint64 MaterialID = (MaterialPtr & 0xFFFFFFFull) << 28;
                                uint64 SortKey = MaterialID | MeshID;

                                if (BatchedDraws.find(SortKey) == BatchedDraws.end())
                                {
                                    MeshDrawCommands.emplace_back(FMeshDrawCommand
                                    {
                                        .Material           = Material,
                                        .IndexBuffer        = Mesh->GetIndexBuffer(),
                                        .VertexBuffer       = Mesh->GetVertexBuffer(),
                                        .IndirectDrawOffset = (uint32)IndirectDrawArguments.size()
                                    });

                                    IndirectDrawArguments.emplace_back(FDrawIndexedIndirectArguments
                                    {
                                        .IndexCount             = Surface.IndexCount,
                                        .InstanceCount          = (uint32)MeshComponent.Instances.size(),
                                        .StartIndexLocation     = Surface.StartIndex,
                                        .BaseVertexLocation     = 0,
                                        .StartInstanceLocation  = 0,
                                    });
                                    
                                    for (const FTransform& Transform : MeshComponent.Instances)
                                    {
                                        glm::mat4 Matrix        = Transform.GetMatrix();
                                        FAABB BoundingBox       = Mesh->GetAABB().ToWorld(Matrix);
                                        glm::vec3 Center        = (BoundingBox.Min + BoundingBox.Max) * 0.5f;
                                        glm::vec3 Extents       = BoundingBox.Max - Center;
                                        float Radius            = glm::length(Extents);
                                        glm::vec4 SphereBounds  = glm::vec4(Center, Radius);
                                        
                                        glm::uvec4      PackedID;
                                        PackedID.x      = (uint32)entity;
                                        PackedID.y      = (uint32)BatchedDraws[SortKey];
                                        PackedID.z      = 0;
                                        
                                        if (entity == World->GetSelectedEntity())
                                        {
                                            PackedID.z = true;
                                        }
                                
                                        InstanceData.emplace_back(FInstanceData
                                        {
                                            .Transform      = Matrix,
                                            .SphereBounds   = SphereBounds,
                                            .PackedID       = PackedID,
                                        });
                                    }
                                }
                            }
                        }
                    });
                }

                // Give each indirect draw command the correct start instance offset index.
                uint32 CumulativeInstanceCount = 0;
                for (FDrawIndexedIndirectArguments& Arg : IndirectDrawArguments)
                {
                    Arg.StartInstanceLocation = CumulativeInstanceCount;
                    CumulativeInstanceCount += Arg.InstanceCount;
                }

                // Since this value will be written in the shader, we no longer need it, since we've generated unique StartInstanceIndex per draw.
                // It must be reset to 0 because the computer shader atomically increments it, assuming 0 as the start.
                for (FDrawIndexedIndirectArguments& DrawArgs : IndirectDrawArguments)
                {
                    DrawArgs.InstanceCount = 0;
                }
            }
    
            //========================================================================================================================
            
            {
                auto Group = World->GetEntityRegistry().group<FLineBatcherComponent>();
                Group.each([&](FLineBatcherComponent& LineBatcherComponent)
                {
                    for (const auto& Pair : LineBatcherComponent.BatchedLines)
                    {
                        for (const FBatchedLine& Line : Pair.second)
                        {
                            SimpleVertices.emplace_back(glm::vec4(Line.Start, 1.0f), Line.Color);
                            SimpleVertices.emplace_back(glm::vec4(Line.End, 1.0f), Line.Color);
                        }
                    }
        
                    LineBatcherComponent.Flush();
                });
            }
            
            //========================================================================================================================

            {
                LightData.bHasSun = false;
                
                auto View = World->GetEntityRegistry().view<SDirectionalLightComponent>();
                View.each([this](const SDirectionalLightComponent& DirectionalLightComponent)
                {
                    LightData.bHasSun = true;

                    // Setup light data
                    FLight Light;
                    Light.Flags         = LIGHT_TYPE_DIRECTIONAL;
                    Light.Color         = PackColor(DirectionalLightComponent.Color, DirectionalLightComponent.Intensity);
                    Light.Direction     = glm::normalize(DirectionalLightComponent.Direction);

                    const FViewVolume& ViewVolume = SceneViewport->GetViewVolume();

                    float NearClip = ViewVolume.GetNear();
                    float FarClip = ViewVolume.GetFar();
                    float ClipRange = FarClip - NearClip;

                    float MinZ = NearClip;
                    float MaxZ = NearClip + ClipRange;

                    float Range = MaxZ - MinZ;
                    float Ratio = MaxZ / MinZ;

                    float CascadeSplits[NumCascades];
                    for (uint32 i = 0; i < NumCascades; i++)
                    {
                        float P = ((float)i + 1) / (float)NumCascades;
                        float Log = MinZ * glm::pow(Ratio, P);
                        float Uniform = MinZ + Range * P;
                        float D = RenderSettings.CascadeSplitLambda * (Log - Uniform) + Uniform;
                        CascadeSplits[i] = (D - NearClip) / ClipRange;
                    }

                    // For each cascade
                    float LastSplitDist = 0.0;
                    for (int i = 0; i < NumCascades; ++i)
                    {
                        float SplitDist = CascadeSplits[i];
                        LightData.CascadeSplits[i] = SplitDist;

                        FShadowCascade& Cascade = ShadowCascades[i];
                        Cascade.DirectionalLight = Light;

                        glm::vec3 FrustumCorners[8];
                        FFrustum::ComputeFrustumCorners(ViewVolume.ToReverseDepthViewProjectionMatrix(), FrustumCorners);


                        for (uint32 j = 0; j < 4; j++)
                        {
                            glm::vec3 dist = FrustumCorners[j + 4] - FrustumCorners[j];
                            FrustumCorners[j + 4] = FrustumCorners[j] + (dist * SplitDist);
                            FrustumCorners[j] = FrustumCorners[j] + (dist * LastSplitDist);
                        }

                        glm::vec3 FrustumCenter = glm::vec3(0.0f);
                        for (uint32 j = 0; j < 8; j++)
                        {
                            FrustumCenter += FrustumCorners[j];
                        }
                        FrustumCenter /= 8.0f;


                        float Radius = 0.0f;
                        for (uint32 j = 0; j < 8; j++)
                        {
                            float distance = glm::length(FrustumCorners[j] - FrustumCenter);
                            Radius = glm::max(Radius, distance);
                        }
                        Radius = glm::ceil(Radius * 16.0f) / 16.0f;

                        glm::vec3 MaxExtents = glm::vec3(Radius);
                        glm::vec3 MinExtents = -MaxExtents;

                        glm::vec3 LightDir = -Light.Direction;
                        glm::mat4 LightViewMatrix = glm::lookAt(FrustumCenter - LightDir * -MinExtents.z, FrustumCenter, FViewVolume::UpAxis);

                        glm::vec3 FrustumCenterLS = LightViewMatrix * glm::vec4(FrustumCenter, 1.0f);
                        float TexelSize = (Radius * 2.0f) / (float)GShadowMapResolution;
                        FrustumCenterLS.x = glm::floor(FrustumCenterLS.x / TexelSize) * TexelSize;
                        FrustumCenterLS.y = glm::floor(FrustumCenterLS.y / TexelSize) * TexelSize;

                        glm::mat4 InvLightViewMatrix = glm::inverse(LightViewMatrix);
                        glm::vec3 SnappedFrustumCenter = InvLightViewMatrix * glm::vec4(FrustumCenterLS, 1.0f);

                        LightViewMatrix = glm::lookAt(SnappedFrustumCenter - LightDir * -MinExtents.z, SnappedFrustumCenter, FViewVolume::UpAxis);

                        glm::mat4 LightOrthoMatrix = glm::ortho(MinExtents.x, MaxExtents.x, MinExtents.y, MaxExtents.y, 0.0f, MaxExtents.z - MinExtents.z);

                        Cascade.SplitDepth = (NearClip + SplitDist * ClipRange) * -1.0f;
                        Cascade.LightViewProjection = LightOrthoMatrix * LightViewMatrix;

                        Light.ViewProjection[i] = Cascade.LightViewProjection;

                        Cascade.ShadowMapSize = glm::ivec2(GShadowMapResolution);

                        LastSplitDist = CascadeSplits[i];
                    }

                    LightData.SunDirection = Light.Direction;
                    LightData.NumLights++;
                    
                    LightData.Lights[0] = Memory::Move(Light);
                });
            }

            //========================================================================================================================

            {
                int32 CurrentShadowMapIndex = 0;
                auto Group = World->GetEntityRegistry().group<SPointLightComponent>(entt::get<STransformComponent>);
                for (uint64 i = 0; i < Group.size(); ++i)
                {
                    entt::entity entity = Group[i];
                    const SPointLightComponent& PointLightComponent = Group.get<SPointLightComponent>(entity);
                    const STransformComponent& TransformComponent = Group.get<STransformComponent>(entity);
        
                    FLight Light;
                    Light.Flags                 = LIGHT_TYPE_POINT;
                    Light.Falloff               = PointLightComponent.Falloff;
                    Light.Color                 = PackColor(PointLightComponent.LightColor, PointLightComponent.Intensity);
                    Light.Radius                = PointLightComponent.Attenuation;
                    Light.Position              = TransformComponent.WorldTransform.Location;

                                        
                    FViewVolume LightView(90.0f, 1.0f, 0.01f, Light.Radius);
                    
                    auto SetView = [&Light](FViewVolume& View, uint32 Index)
                    {
                        switch (Index)
                        {
                        case 0: // + X
                            View.SetView(Light.Position, FViewVolume::RightAxis, FViewVolume::DownAxis);
                            break;
                        case 1: // - X
                            View.SetView(Light.Position, FViewVolume::LeftAxis, FViewVolume::DownAxis);
                            break;
                        case 2: // + Y
                            View.SetView(Light.Position, FViewVolume::UpAxis, FViewVolume::ForwardAxis);
                            break;
                        case 3: // - Y
                            View.SetView(Light.Position, FViewVolume::DownAxis, FViewVolume::BackwardAxis);
                            break;
                        case 4: // + Z
                            View.SetView(Light.Position, FViewVolume::ForwardAxis, FViewVolume::DownAxis);
                            break;
                        case 5: // - Z
                            View.SetView(Light.Position, FViewVolume::BackwardAxis, FViewVolume::DownAxis);
                            break;
                        default:
                            LUMINA_NO_ENTRY()
                        }
                    };

                    for (int Face = 0; Face < 6; ++Face)
                    {
                        SetView(LightView, Face);
                        Light.ViewProjection[Face] = LightView.ToReverseDepthViewProjectionMatrix();                        
                    }
                    
                    if (PointLightComponent.bCastShadows)
                    {
                        Light.Shadow.ShadowMapIndex = CurrentShadowMapIndex++;
                    }
                    else
                    {
                        Light.Shadow.ShadowMapIndex = INDEX_NONE;
                    }
                    
                    LightData.Lights[LightData.NumLights++] = Memory::Move(Light);
        
                    //Scene->DrawDebugSphere(Transform.Location, 0.25f, Light.Color);
                }
            }
        
            //========================================================================================================================
            
            {
                int32 CurrentShadowMapIndex = 0;
                auto Group = World->GetEntityRegistry().group<SSpotLightComponent>(entt::get<STransformComponent>);
                for (uint64 i = 0; i < Group.size(); ++i)
                {
                    entt::entity entity = Group[i];
                    const SSpotLightComponent& SpotLightComponent = Group.get<SSpotLightComponent>(entity);
                    const FTransform& Transform = Group.get<STransformComponent>(entity).WorldTransform;

                    glm::vec3 UpdatedForward    = Transform.Rotation * FViewVolume::ForwardAxis;
                    glm::vec3 UpdatedUp         = Transform.Rotation * FViewVolume::UpAxis;

                    float InnerDegrees = SpotLightComponent.InnerConeAngle;
                    float OuterDegrees = SpotLightComponent.OuterConeAngle;
        
                    float InnerCos = glm::cos(glm::radians(InnerDegrees));
                    float OuterCos = glm::cos(glm::radians(OuterDegrees));
                    
                    FViewVolume ViewVolume(OuterDegrees * 2.00f, 1.0f, 0.01f, SpotLightComponent.Attenuation);
                    ViewVolume.SetView(Transform.Location, -UpdatedForward, UpdatedUp);
                    
                    FLight Light;
                    Light.Flags                 = LIGHT_TYPE_SPOT;
                    Light.Position              = Transform.Location;
                    Light.Direction             = glm::normalize(UpdatedForward);
                    Light.Falloff               = SpotLightComponent.Falloff;
                    Light.Color                 = PackColor(SpotLightComponent.LightColor, SpotLightComponent.Intensity);
                    Light.Radius                = SpotLightComponent.Attenuation;
                    Light.Angles                = glm::vec2(InnerCos, OuterCos);
                    Light.ViewProjection[0]     = ViewVolume.ToReverseDepthViewProjectionMatrix();

                    if (SpotLightComponent.bCastShadows)
                    {
                        int32 TileIndex = ShadowAtlas.AllocateTile();
                        if (TileIndex != INDEX_NONE)
                        {
                            const FShadowTile& Tile = ShadowAtlas.GetTile(TileIndex);
                            Light.Shadow.ShadowMapIndex = TileIndex;
                            Light.Shadow.AtlasUVOffset = Tile.UVOffset;
                            Light.Shadow.AtlasUVScale = Tile.UVScale;
                        }
                    }
                    else
                    {
                        Light.Shadow.ShadowMapIndex = INDEX_NONE;
                    }

                    LightData.Lights[LightData.NumLights++] = Memory::Move(Light);
                    
                   //World->DrawViewVolume(ViewVolume, FColor::Red);
        
                   //World->DrawDebugCone(SpotLight.Position, Forward, glm::radians(OuterDegrees), SpotLightComponent.Attenuation, glm::vec4(SpotLightComponent.LightColor, 1.0f));
                   //World->DrawDebugCone(SpotLight.Position, Forward, glm::radians(InnerDegrees), SpotLightComponent.Attenuation, glm::vec4(SpotLightComponent.LightColor, 1.0f));
        
                }
            }
        
            
            //========================================================================================================================
        
                
            {
                RenderSettings.bHasEnvironment = false;
                LightData.AmbientLight = glm::vec4(0.0f);
                RenderSettings.bSSAO = false;
                auto View = World->GetEntityRegistry().view<SEnvironmentComponent>();
                View.each([this] (const SEnvironmentComponent& EnvironmentComponent)
                {
                    LightData.AmbientLight                              = glm::vec4(EnvironmentComponent.AmbientLight.Color, EnvironmentComponent.AmbientLight.Intensity);
                    RenderSettings.bHasEnvironment                      = true;
                    RenderSettings.bSSAO                                = EnvironmentComponent.bSSAOEnabled;
                    SceneGlobalData.SSAOSettings.Intensity              = EnvironmentComponent.SSAOInfo.Intensity;
                    SceneGlobalData.SSAOSettings.Power                  = EnvironmentComponent.SSAOInfo.Power;
                    SceneGlobalData.SSAOSettings.Radius                 = EnvironmentComponent.SSAOInfo.Radius;
                });
            }

            {
                FRGPassDescriptor* Descriptor = RenderGraph.AllocDescriptor();
                Descriptor->AddRawWrite(SceneDataBuffer);
                Descriptor->AddRawWrite(InstanceDataBuffer);
                Descriptor->AddRawWrite(IndirectDrawBuffer);
                Descriptor->AddRawWrite(SimpleVertexBuffer);
                Descriptor->AddRawWrite(LightDataBuffer);
                
                RenderGraph.AddPass<RG_Raster>(FRGEvent("Write Scene Buffers"), Descriptor, [&](ICommandList& CmdList)
                {
                    LUMINA_PROFILE_SECTION_COLORED("Write Scene Buffers", tracy::Color::OrangeRed3);

                    CheckInstanceBufferResize(InstanceData.size());
                    
                    const SIZE_T SimpleVertexSize = SimpleVertices.size() * sizeof(FSimpleElementVertex);
                    const SIZE_T InstanceDataSize = InstanceData.size() * sizeof(FInstanceData);
                    const SIZE_T IndirectArgsSize = IndirectDrawArguments.size() * sizeof(FDrawIndexedIndirectArguments);

                    constexpr SIZE_T LightDataHeaderSize = offsetof(FSceneLightData, Lights);
                    const SIZE_T ActiveLightsSize = LightData.NumLights * sizeof(FLight);
                    const SIZE_T LightUploadSize = LightDataHeaderSize + ActiveLightsSize;
                    
                    CmdList.SetBufferState(SceneDataBuffer, EResourceStates::CopyDest);
                    CmdList.SetBufferState(InstanceDataBuffer, EResourceStates::CopyDest);
                    CmdList.SetBufferState(IndirectDrawBuffer, EResourceStates::CopyDest);
                    CmdList.SetBufferState(SimpleVertexBuffer, EResourceStates::CopyDest);
                    CmdList.SetBufferState(LightDataBuffer, EResourceStates::CopyDest);
                    CmdList.CommitBarriers();
                    
                    CmdList.DisableAutomaticBarriers();
                    CmdList.WriteBuffer(SceneDataBuffer, &SceneGlobalData, 0, sizeof(FSceneGlobalData));
                    CmdList.WriteBuffer(InstanceDataBuffer, InstanceData.data(), 0, InstanceDataSize);
                    CmdList.WriteBuffer(IndirectDrawBuffer, IndirectDrawArguments.data(), 0, IndirectArgsSize);
                    CmdList.WriteBuffer(SimpleVertexBuffer, SimpleVertices.data(), 0, SimpleVertexSize);
                    CmdList.WriteBuffer(LightDataBuffer, &LightData, 0, LightUploadSize);
                    CmdList.EnableAutomaticBarriers();
                });
            }
        }
        
        void FRenderScene::InitResources()
        {
            InitBuffers();
            CreateImages();

            {
                FVertexAttributeDesc VertexDesc[4];
                // Pos
                VertexDesc[0].SetElementStride(sizeof(FVertex));
                VertexDesc[0].SetOffset(offsetof(FVertex, Position));
                VertexDesc[0].Format = EFormat::RGB32_FLOAT;

                // Normal
                VertexDesc[1].SetElementStride(sizeof(FVertex));
                VertexDesc[1].SetOffset(offsetof(FVertex, Normal));
                VertexDesc[1].Format = EFormat::R32_UINT;

                // UV
                VertexDesc[2].SetElementStride(sizeof(FVertex));
                VertexDesc[2].SetOffset(offsetof(FVertex, UV));
                VertexDesc[2].Format = EFormat::RG16_UINT;
                
                // Color
                VertexDesc[3].SetElementStride(sizeof(FVertex));
                VertexDesc[3].SetOffset(offsetof(FVertex, Color));
                VertexDesc[3].Format = EFormat::RGBA8_UNORM;

            
                VertexLayoutInput = GRenderContext->CreateInputLayout(VertexDesc, std::size(VertexDesc));
            }

            {
                FVertexAttributeDesc VertexDesc[1];
                // Pos
                VertexDesc[0].SetElementStride(sizeof(FVertex));
                VertexDesc[0].SetOffset(offsetof(FVertex, Position));
                VertexDesc[0].Format = EFormat::RGB32_FLOAT;
                
                PositionOnlyLayoutInput = GRenderContext->CreateInputLayout(VertexDesc, 1);
            }

            {
                FVertexAttributeDesc VertexDesc[2];
                // Pos
                VertexDesc[0].SetElementStride(sizeof(FSimpleElementVertex));
                VertexDesc[0].SetOffset(offsetof(FSimpleElementVertex, Position));
                VertexDesc[0].Format = EFormat::RGBA32_FLOAT;

                // Color
                VertexDesc[1].SetElementStride(sizeof(FSimpleElementVertex));
                VertexDesc[1].SetOffset(offsetof(FSimpleElementVertex, Color));
                VertexDesc[1].Format = EFormat::RGBA32_FLOAT;

                SimpleVertexLayoutInput = GRenderContext->CreateInputLayout(VertexDesc, std::size(VertexDesc));
            }

            {
                FBindingSetDesc BindingSetDesc;
                BindingSetDesc.AddItem(FBindingSetItem::BufferSRV(0, InstanceDataBuffer));
                BindingSetDesc.AddItem(FBindingSetItem::BufferUAV(1, InstanceMappingBuffer));
                BindingSetDesc.AddItem(FBindingSetItem::BufferUAV(2, IndirectDrawBuffer));
                BindingSetDesc.AddItem(FBindingSetItem::PushConstants(0, sizeof(FCullData)));

                TBitFlags<ERHIShaderType> Visibility;
                Visibility.SetMultipleFlags(ERHIShaderType::Compute);
                GRenderContext->CreateBindingSetAndLayout(Visibility, 0, BindingSetDesc, FrustumCullLayout, FrustumCullSet);
            }
            
            {

                FBindingSetDesc BindingSetDesc;
                BindingSetDesc.AddItem(FBindingSetItem::BufferCBV(0, SceneDataBuffer));
                BindingSetDesc.AddItem(FBindingSetItem::BufferSRV(1, InstanceDataBuffer));
                BindingSetDesc.AddItem(FBindingSetItem::BufferSRV(2, InstanceMappingBuffer));
                BindingSetDesc.AddItem(FBindingSetItem::BufferSRV(3, LightDataBuffer));
                //BindingSetDesc.AddItem(FBindingSetItem::TextureUAV(5, OverdrawImage));

                TBitFlags<ERHIShaderType> Visibility;
                Visibility.SetMultipleFlags(ERHIShaderType::Vertex, ERHIShaderType::Fragment);
                GRenderContext->CreateBindingSetAndLayout(Visibility, 0, BindingSetDesc, BindingLayout, BindingSet);
            }

            {
                FBindingSetDesc SetDesc;
                SetDesc.AddItem(FBindingSetItem::TextureSRV(0, DepthAttachment));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(1, GBuffer.Normals));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(2, GBuffer.Material));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(3, GBuffer.AlbedoSpec));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(4, SSAOBlur));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(5, CascadedShadowMap));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(6, PointLightShadowMap, TStaticRHISampler<true, AM_Border, AM_Border, AM_Border>::GetRHI()));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(7, ShadowAtlas.GetImage(), TStaticRHISampler<true, AM_Border, AM_Border, AM_Border>::GetRHI()));
                SetDesc.AddItem(FBindingSetItem::BufferSRV(8, ClusterBuffer));

                TBitFlags<ERHIShaderType> Visibility;
                Visibility.SetMultipleFlags(ERHIShaderType::Fragment);
                GRenderContext->CreateBindingSetAndLayout(Visibility, 0, SetDesc, LightingPassLayout, LightingPassSet);

            }

            {
                FBindingSetDesc SetDesc;
                SetDesc.AddItem(FBindingSetItem::TextureSRV(0, GetRenderTarget()));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(1, DepthAttachment));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(2, GBuffer.Normals));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(3, GBuffer.AlbedoSpec));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(4, SSAOBlur));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(5, GBuffer.Material));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(6, ShadowAtlas.GetImage()));

                SetDesc.AddItem(FBindingSetItem::PushConstants(0, sizeof(uint32)));

                TBitFlags<ERHIShaderType> Visibility;
                Visibility.SetMultipleFlags(ERHIShaderType::Fragment);
                GRenderContext->CreateBindingSetAndLayout(Visibility, 0, SetDesc, DebugPassLayout, DebugPassSet);
            }

            {
                FBindingLayoutDesc LayoutDesc;
                LayoutDesc.StageFlags.SetFlag(ERHIShaderType::Vertex);
                LayoutDesc.AddItem(FBindingLayoutItem::PushConstants(0, 80));
                SimplePassLayout = GRenderContext->CreateBindingLayout(LayoutDesc);
            }

            {
                FBindingSetDesc SetDesc;
                SetDesc.AddItem(FBindingSetItem::BufferUAV(0, ClusterBuffer));
                SetDesc.AddItem(FBindingSetItem::PushConstants(0, sizeof(FLightClusterPC)));

                TBitFlags<ERHIShaderType> Visibility;
                Visibility.SetMultipleFlags(ERHIShaderType::Compute);
                GRenderContext->CreateBindingSetAndLayout(Visibility, 0, SetDesc, ClusterBuildLayout, ClusterBuildSet);

            }

            {
                FBindingSetDesc SetDesc;
                SetDesc.AddItem(FBindingSetItem::BufferUAV(0, ClusterBuffer));
                SetDesc.AddItem(FBindingSetItem::BufferUAV(1, LightDataBuffer));
                SetDesc.AddItem(FBindingSetItem::PushConstants(0, sizeof(glm::mat4)));

                TBitFlags<ERHIShaderType> Visibility;
                Visibility.SetMultipleFlags(ERHIShaderType::Compute);
                GRenderContext->CreateBindingSetAndLayout(Visibility, 0, SetDesc, LightCullLayout, LightCullSet);

            }

            {
                FBindingSetDesc SetDesc;
                SetDesc.AddItem(FBindingSetItem::PushConstants(0, sizeof(uint32)));

                TBitFlags<ERHIShaderType> Visibility;
                Visibility.SetMultipleFlags(ERHIShaderType::Vertex, ERHIShaderType::Fragment);
                GRenderContext->CreateBindingSetAndLayout(Visibility, 0, SetDesc, ShadowPassLayout, ShadowPassSet);

            }

            {
                FBindingSetDesc SetDesc;
                SetDesc.AddItem(FBindingSetItem::TextureSRV(0, DepthAttachment));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(1, GBuffer.Normals));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(2, NoiseImage, TStaticRHISampler<true, AM_Repeat, AM_Repeat, AM_Repeat>::GetRHI()));

                TBitFlags<ERHIShaderType> Visibility;
                Visibility.SetMultipleFlags(ERHIShaderType::Fragment);
                GRenderContext->CreateBindingSetAndLayout(Visibility, 0, SetDesc, SSAOPassLayout, SSAOPassSet);
            }

            {
                FBindingSetDesc SetDesc;
                SetDesc.AddItem(FBindingSetItem::TextureSRV(0, SSAOImage));

                TBitFlags<ERHIShaderType> Visibility;
                Visibility.SetMultipleFlags(ERHIShaderType::Fragment);
                GRenderContext->CreateBindingSetAndLayout(Visibility, 0, SetDesc, SSAOBlurPassLayout, SSAOBlurPassSet);
                
            }
            
        }

        static float lerp(float a, float b, float f)
        {
            return a + f * (b - a);
        }

        void FRenderScene::InitBuffers()
        {
            {
                FRHIBufferDesc BufferDesc;
                BufferDesc.Size = sizeof(FSceneGlobalData);
                BufferDesc.Stride = sizeof(FSceneGlobalData);
                BufferDesc.Usage.SetMultipleFlags(BUF_UniformBuffer);
                BufferDesc.bKeepInitialState = true;
                BufferDesc.InitialState = EResourceStates::ShaderResource;
                BufferDesc.DebugName = "Scene Global Data";
                SceneDataBuffer = GRenderContext->CreateBuffer(BufferDesc);
            }

            {
                FRHIBufferDesc BufferDesc;
                BufferDesc.Size = sizeof(FInstanceData) * 5'000;
                BufferDesc.Usage.SetMultipleFlags(BUF_StorageBuffer);
                BufferDesc.bKeepInitialState = true;
                BufferDesc.InitialState = EResourceStates::ShaderResource;
                BufferDesc.DebugName = "Instance Buffer";
                InstanceDataBuffer = GRenderContext->CreateBuffer(BufferDesc);
            }

            {
                FRHIBufferDesc BufferDesc;
                BufferDesc.Size = sizeof(uint32) * 5'000;
                BufferDesc.Usage.SetMultipleFlags(BUF_StorageBuffer);
                BufferDesc.bKeepInitialState = true;
                BufferDesc.InitialState = EResourceStates::UnorderedAccess;
                BufferDesc.DebugName = "Instance Mapping";
                InstanceMappingBuffer = GRenderContext->CreateBuffer(BufferDesc);
            }

            {
                FRHIBufferDesc BufferDesc;
                BufferDesc.Size = sizeof(FSceneLightData);
                BufferDesc.Stride = sizeof(FSceneLightData);
                BufferDesc.Usage.SetMultipleFlags(BUF_StorageBuffer);
                BufferDesc.bKeepInitialState = true;
                BufferDesc.InitialState = EResourceStates::ShaderResource;
                BufferDesc.DebugName = "Light Data Buffer";
                LightDataBuffer = GRenderContext->CreateBuffer(BufferDesc);
            }

            {
                FRHIBufferDesc BufferDesc;
                BufferDesc.Size = sizeof(FCluster) * NumClusters;
                BufferDesc.Usage.SetMultipleFlags(BUF_StorageBuffer);
                BufferDesc.bKeepInitialState = true;
                BufferDesc.InitialState = EResourceStates::UnorderedAccess;
                BufferDesc.DebugName = "Cluster SSBO";
                ClusterBuffer = GRenderContext->CreateBuffer(BufferDesc);
            }

            {
                
                // SSAO
                std::default_random_engine RndEngine;
                std::uniform_real_distribution RndDist(0.0f, 1.0f);

                // Sample kernel
                for (uint32_t i = 0; i < 32; ++i)
                {
                    glm::vec3 sample(RndDist(RndEngine) * 2.0 - 1.0, RndDist(RndEngine) * 2.0 - 1.0, RndDist(RndEngine));
                    sample = glm::normalize(sample);
                    sample *= RndDist(RndEngine);
                    float scale = float(i) / float(32);
                    scale = lerp(0.1f, 1.0f, scale * scale);
                    SceneGlobalData.SSAOSettings.Samples[i] = glm::vec4(sample * scale, 0.0f);
                }



                FRHICommandListRef CommandList = GRenderContext->CreateCommandList(FCommandListInfo::Graphics());
                CommandList->Open();
            
                FRHIImageDesc SSAONoiseDesc = {};
                SSAONoiseDesc.Extent = {4, 4};
                SSAONoiseDesc.Format = EFormat::RGBA32_FLOAT;
                SSAONoiseDesc.Dimension = EImageDimension::Texture2D;
                SSAONoiseDesc.bKeepInitialState = true;
                SSAONoiseDesc.InitialState = EResourceStates::ShaderResource;
                SSAONoiseDesc.Flags.SetMultipleFlags(EImageCreateFlags::ShaderResource);
                SSAONoiseDesc.DebugName = "SSAO Noise";
            
                NoiseImage = GRenderContext->CreateImage(SSAONoiseDesc);
            
                // Random noise
                TVector<glm::vec4> NoiseValues(32);
                for (SIZE_T i = 0; i < NoiseValues.size(); i++)
                {
                    NoiseValues[i] = glm::vec4(RndDist(RndEngine) * 2.0f - 1.0f, RndDist(RndEngine) * 2.0f - 1.0f, 0.0f, 0.0f);
                }
                
                CommandList->WriteImage(NoiseImage, 0, 0, NoiseValues.data(), 4 * 16, 0);

                CommandList->Close();
                GRenderContext->ExecuteCommandList(CommandList, ECommandQueue::Graphics);
            }
            

            {
                SimpleVertexBuffer = FRHITypedVertexBuffer<FSimpleElementVertex>::CreateEmpty(100'000);
            }

            {
                FRHIBufferDesc BufferDesc;
                BufferDesc.Size = sizeof(FDrawIndexedIndirectArguments) * (10'000);
                BufferDesc.Stride = sizeof(FDrawIndexedIndirectArguments);
                BufferDesc.Usage.SetMultipleFlags(BUF_Indirect, BUF_StorageBuffer);
                BufferDesc.InitialState = EResourceStates::IndirectArgument;
                BufferDesc.bKeepInitialState = true;
                BufferDesc.DebugName = "Indirect Draw Buffer";
                IndirectDrawBuffer = GRenderContext->CreateBuffer(BufferDesc);
            }
        }
        
        void FRenderScene::CreateImages()
        {
            FIntVector2D Extent = Windowing::GetPrimaryWindowHandle()->GetExtent();

            {
                FRHIImageDesc ImageDesc = {};
                ImageDesc.Extent = Extent;
                ImageDesc.Format = EFormat::RGBA32_FLOAT;
                ImageDesc.Dimension = EImageDimension::Texture2D;
                ImageDesc.bKeepInitialState = true;
                ImageDesc.InitialState = EResourceStates::RenderTarget;
                ImageDesc.Flags.SetMultipleFlags(EImageCreateFlags::RenderTarget, EImageCreateFlags::ShaderResource);
                ImageDesc.DebugName = "Debug Visualization";
            
                DebugVisualizationImage = GRenderContext->CreateImage(ImageDesc);
            }
           

            {
                FRHIImageDesc ImageDesc = {};
                ImageDesc.Extent = Extent;
                ImageDesc.Format = EFormat::RGBA16_FLOAT;
                ImageDesc.Dimension = EImageDimension::Texture2D;
                ImageDesc.bKeepInitialState = true;
                ImageDesc.InitialState = EResourceStates::ShaderResource;
                ImageDesc.Flags.SetMultipleFlags(EImageCreateFlags::ColorAttachment, EImageCreateFlags::ShaderResource);
                ImageDesc.DebugName = "GBuffer - Normals";
            
                GBuffer.Normals = GRenderContext->CreateImage(ImageDesc);
            }

            {
                FRHIImageDesc ImageDesc = {};
                ImageDesc.Extent = Extent;
                ImageDesc.Format = EFormat::RGBA8_UNORM;
                ImageDesc.Dimension = EImageDimension::Texture2D;
                ImageDesc.bKeepInitialState = true;
                ImageDesc.InitialState = EResourceStates::ShaderResource;
                ImageDesc.Flags.SetMultipleFlags(EImageCreateFlags::ColorAttachment, EImageCreateFlags::ShaderResource);
                ImageDesc.DebugName = "GBuffer - Material";
            
                GBuffer.Material = GRenderContext->CreateImage(ImageDesc);
            }

            {
                FRHIImageDesc ImageDesc = {};
                ImageDesc.Extent = Extent;
                ImageDesc.Format = EFormat::RGBA8_UNORM;
                ImageDesc.Dimension = EImageDimension::Texture2D;
                ImageDesc.bKeepInitialState = true;
                ImageDesc.InitialState = EResourceStates::ShaderResource;
                ImageDesc.Flags.SetMultipleFlags(EImageCreateFlags::ColorAttachment, EImageCreateFlags::ShaderResource);
                ImageDesc.DebugName = "GBuffer - Albedo";
            
                GBuffer.AlbedoSpec = GRenderContext->CreateImage(ImageDesc);
            }
            

            {
                FRHIImageDesc ImageDesc;
                ImageDesc.Extent = Extent;
                ImageDesc.Flags.SetMultipleFlags(EImageCreateFlags::DepthAttachment, EImageCreateFlags::ShaderResource);
                ImageDesc.Format = EFormat::D32;
                ImageDesc.InitialState = EResourceStates::DepthWrite;
                ImageDesc.bKeepInitialState = true;
                ImageDesc.Dimension = EImageDimension::Texture2D;
                ImageDesc.DebugName = "Depth Attachment";

                DepthAttachment = GRenderContext->CreateImage(ImageDesc);
            }

            {
                FRHIImageDesc ImageDesc = {};
                ImageDesc.Extent = Extent;
                ImageDesc.Format = EFormat::R8_UNORM;
                ImageDesc.Dimension = EImageDimension::Texture2D;
                ImageDesc.bKeepInitialState = true;
                ImageDesc.InitialState = EResourceStates::ShaderResource;
                ImageDesc.Flags.SetMultipleFlags(EImageCreateFlags::ColorAttachment, EImageCreateFlags::ShaderResource);
                ImageDesc.DebugName = "SSAO";
            
                SSAOImage = GRenderContext->CreateImage(ImageDesc);
            }

            {
                FRHIImageDesc ImageDesc = {};
                ImageDesc.Extent = Extent;
                ImageDesc.Format = EFormat::R8_UNORM;
                ImageDesc.Dimension = EImageDimension::Texture2D;
                ImageDesc.bKeepInitialState = true;
                ImageDesc.InitialState = EResourceStates::ShaderResource;
                ImageDesc.Flags.SetMultipleFlags(EImageCreateFlags::ColorAttachment, EImageCreateFlags::ShaderResource);
                ImageDesc.DebugName = "SSAO Blur";
            
                SSAOBlur = GRenderContext->CreateImage(ImageDesc);
            }

            {
                FRHIImageDesc ImageDesc = {};
                ImageDesc.Extent = Extent;
                ImageDesc.Format = EFormat::R32_UINT;
                ImageDesc.Dimension = EImageDimension::Texture2D;
                ImageDesc.bKeepInitialState = true;
                ImageDesc.InitialState = EResourceStates::UnorderedAccess;
                ImageDesc.Flags.SetMultipleFlags(EImageCreateFlags::ShaderResource, EImageCreateFlags::UnorderedAccess);
                ImageDesc.DebugName = "Overdraw Visualization";
            
                OverdrawImage = GRenderContext->CreateImage(ImageDesc);
            }

            {
                FRHIImageDesc ImageDesc = {};
                ImageDesc.Extent = Extent;
                ImageDesc.Format = EFormat::R32_UINT;
                ImageDesc.Dimension = EImageDimension::Texture2D;
                ImageDesc.InitialState = EResourceStates::RenderTarget;
                ImageDesc.bKeepInitialState = true;
                ImageDesc.Flags.SetMultipleFlags(EImageCreateFlags::ColorAttachment, EImageCreateFlags::ShaderResource);
                ImageDesc.DebugName = "Picker";
                
                PickerImage = GRenderContext->CreateImage(ImageDesc);
            }

            {
                FRHIImageDesc ImageDesc;
                ImageDesc.Extent = Extent;
                ImageDesc.Flags.SetMultipleFlags(EImageCreateFlags::DepthAttachment, EImageCreateFlags::ShaderResource);
                ImageDesc.Format = EFormat::D32;
                ImageDesc.InitialState = EResourceStates::DepthWrite;
                ImageDesc.bKeepInitialState = true;
                ImageDesc.Dimension = EImageDimension::Texture2D;
                ImageDesc.DebugName = "Depth Map";

                DepthMap = GRenderContext->CreateImage(ImageDesc);
            }
            
            //==================================================================================================
            
            {
                FRHIImageDesc ImageDesc = {};
                ImageDesc.Extent = FIntVector2D(4096, 4096);
                ImageDesc.Format = EFormat::D32;
                ImageDesc.Dimension = EImageDimension::Texture2DArray;
                ImageDesc.InitialState = EResourceStates::DepthWrite;
                ImageDesc.bKeepInitialState = true;
                ImageDesc.ArraySize = NumCascades;
                ImageDesc.Flags.SetMultipleFlags(EImageCreateFlags::DepthAttachment, EImageCreateFlags::ShaderResource);
                ImageDesc.DebugName = "ShadowCascadeMap";
                
                CascadedShadowMap = GRenderContext->CreateImage(ImageDesc);
            }
            
            //==================================================================================================
            
            {
                FRHIImageDesc ImageDesc;
                ImageDesc.Extent = {GShadowCubemapResolution, GShadowCubemapResolution};
                ImageDesc.Flags.SetMultipleFlags(EImageCreateFlags::DepthAttachment, EImageCreateFlags::ShaderResource, EImageCreateFlags::CubeCompatible);
                ImageDesc.Dimension = EImageDimension::TextureCubeArray;
                ImageDesc.ArraySize = 6 * GMaxPointLightShadows;
                ImageDesc.Format = EFormat::D16;
                ImageDesc.bKeepInitialState = true;
                ImageDesc.InitialState = EResourceStates::DepthWrite;
                ImageDesc.DebugName = "Point Light Shadow Cubemap";

                PointLightShadowMap = GRenderContext->CreateImage(ImageDesc);
            }
        }
    }
