#include "RenderScene.h"
#include <execution>
#include <variant>
#include "Assets/AssetRegistry/AssetRegistry.h"
#include "Assets/AssetTypes/Mesh/StaticMesh/StaticMesh.h"
#include "Core/Windows/Window.h"
#include "Renderer/RHIIncl.h"
#include "Assets/AssetTypes/Material/Material.h"
#include "Assets/AssetTypes/Textures/Texture.h"
#include "Core/Engine/Engine.h"
#include "Core/Profiler/Profile.h"
#include "EASTL/sort.h"
#define GLM_ENABLE_EXPERIMENTAL
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

        constexpr int GShadowMapResolution = 4096;
        
        FRenderScene::FRenderScene(CWorld* InWorld)
            : World(InWorld)
            , SceneRenderStats()
            , SceneGlobalData()
        {
            DebugVisualizationMode = ERenderSceneDebugFlags::None;
            
            SceneViewport = GRenderContext->CreateViewport(Windowing::GetPrimaryWindowHandle()->GetExtent());
            
            LOG_TRACE("Initializing Renderer Scene");

            
            InitBuffers();
            CreateImages();
            InitResources();
        }

        FRenderScene::~FRenderScene()
        {
            GRenderContext->WaitIdle();
            
            LOG_TRACE("Shutting down Renderer Scene");
        }
        
        void FRenderScene::RenderScene(FRenderGraph& RenderGraph)
        {
            LUMINA_PROFILE_SCOPE();
            SceneRenderStats = {};
            
            SCameraComponent& CameraComponent = World->GetActiveCamera();
            STransformComponent& CameraTransform = World->GetActiveCameraEntity().GetComponent<STransformComponent>();

            glm::vec3 UpdatedForward = CameraTransform.Transform.Rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            glm::vec3 UpdatedUp      = CameraTransform.Transform.Rotation * glm::vec3(0.0f, 1.0f,  0.0f);
        
            CameraComponent.SetView(CameraTransform.Transform.Location, CameraTransform.Transform.Location + UpdatedForward, UpdatedUp);

            SetViewVolume(CameraComponent.GetViewVolume());

            // Wait for shader tasks.
            if(GRenderContext->GetShaderCompiler()->HasPendingRequests())
            {
                /** Mostly for UI rendering */  
                RenderGraph.AddPass<RG_Raster>(FRGEvent("Finalize Image State"), nullptr, [&](ICommandList& CmdList)
                {
                    CmdList.SetImageState(GetVisualizationImage(), AllSubresources, EResourceStates::ShaderResource);
                    CmdList.CommitBarriers();
                });
                
                return;
            }


            ResetPass(RenderGraph);
            CompileDrawCommands();
            CullPass(RenderGraph, SceneViewport->GetViewVolume());
            DepthPrePass(RenderGraph, SceneViewport->GetViewVolume());
            ShadowMappingPass(RenderGraph);
            GBufferPass(RenderGraph, SceneViewport->GetViewVolume());
            SSAOPass(RenderGraph);
            EnvironmentPass(RenderGraph);
            DeferredLightingPass(RenderGraph);
            BatchedLineDraw(RenderGraph);
            DebugDrawPass(RenderGraph);
            
            /** Mostly for UI rendering */  
            RenderGraph.AddPass<RG_Raster>(FRGEvent("Finalize Image State"), nullptr, [&](ICommandList& CmdList)
            {
                CmdList.SetImageState(GetVisualizationImage(), AllSubresources, EResourceStates::ShaderResource);
                CmdList.CommitBarriers();
            });
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
            RenderGraph.AddPass<RG_Raster>(FRGEvent("Cull Pass"), nullptr, [&] (ICommandList& CmdList)
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
                CullData.Frustum = SceneViewport->GetViewVolume().GetFrustum();
                CullData.View = glm::vec4(SceneViewport->GetViewVolume().GetViewPosition(), (uint32)InstanceData.size());
                
                CmdList.SetPushConstants(&CullData, sizeof(FCullData));

                uint32 Num = (uint32)InstanceData.size();
                uint32 NumWorkGroups = (Num + 255) / 256;
                
                CmdList.Dispatch(NumWorkGroups, 1, 1);
                
            });
        }

        void FRenderScene::DepthPrePass(FRenderGraph& RenderGraph, const FViewVolume& View)
        {
            RenderGraph.AddPass<RG_Raster>(FRGEvent("Pre-Depth Pass"), nullptr, [&] (ICommandList& CmdList)
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
                    .SetInputLayout(VertexLayoutInput)
                    .AddBindingLayout(BindingLayout)
                    .SetVertexShader(VertexShader);
                    
                FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc);
                
                for (SIZE_T CurrentDraw = 0; CurrentDraw < MeshDrawCommands.size(); ++CurrentDraw)
                {
                    const FMeshDrawCommand& Batch = MeshDrawCommands[CurrentDraw];
                    
                    FGraphicsState GraphicsState;
                    GraphicsState.AddVertexBuffer({ Batch.VertexBuffer });
                    GraphicsState.SetIndexBuffer({ Batch.IndexBuffer });
                    GraphicsState.SetRenderPass(RenderPass);
                    GraphicsState.SetViewport(MakeViewportStateFromImage(GetRenderTarget()));
                    GraphicsState.SetPipeline(Pipeline);
                    GraphicsState.AddBindingSet(BindingSet);
                    GraphicsState.SetIndirectParams(IndirectDrawBuffer);
                    
                    CmdList.SetGraphicsState(GraphicsState);
                    
                    CmdList.DrawIndexedIndirect(1, Batch.IndirectDrawOffset * sizeof(FDrawIndexedIndirectArguments));
                }
            });
        }

        void FRenderScene::ShadowMappingPass(FRenderGraph& RenderGraph)
        {
            RenderGraph.AddPass<RG_Raster>(FRGEvent("Cascaded Shadow Map Pass"), nullptr, [&](ICommandList& CmdList)
            {
                LUMINA_PROFILE_SECTION_COLORED("Cascaded Shadow Map Pass", tracy::Color::DeepPink2);

                if (!bHasDirectionalLight)
                {
                    return;
                }

                FRHIVertexShaderRef VertexShader = FShaderLibrary::GetVertexShader("ShadowMapping.vert");
                if (!VertexShader)
                {
                    return;
                }
                
                
                for (int i = 0; i < NumCascades; ++i)
                {
                    FShadowCascade& Cascade = ShadowCascades[i];

                    FRenderPassDesc::FAttachment Depth; Depth
                        .SetImage(Cascade.ShadowMapImage)
                        .SetDepthClearValue(1.0f);
                
                    FRenderPassDesc RenderPass; RenderPass
                        .SetDepthAttachment(Depth, FTextureSubresourceSet(0, 1, i, 1))
                        .SetRenderArea(FIntVector2D(Cascade.ShadowMapSize.x, Cascade.ShadowMapSize.y));
                    
                    
                    FRenderState RenderState; RenderState
                        .SetDepthStencilState(FDepthStencilState().SetDepthFunc(EComparisonFunc::LessOrEqual))
                        .SetRasterState(FRasterState().EnableDepthClip());
                            
                    FGraphicsPipelineDesc Desc; Desc
                        .SetRenderState(RenderState)
                        .SetInputLayout(VertexLayoutInput)
                        .AddBindingLayout(BindingLayout)
                        .AddBindingLayout(ShadowPassLayout)
                        .SetVertexShader(VertexShader);

                    
                    FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc);
                
                    for (SIZE_T CurrentDraw = 0; CurrentDraw < MeshDrawCommands.size(); ++CurrentDraw)
                    {
                        const FMeshDrawCommand& Batch = MeshDrawCommands[CurrentDraw];
                    
                        FGraphicsState GraphicsState; GraphicsState
                            .AddVertexBuffer({ Batch.VertexBuffer })
                            .SetIndexBuffer({ Batch.IndexBuffer })
                            .SetRenderPass(RenderPass)
                            .SetViewport(MakeViewportStateFromImage(Cascade.ShadowMapImage))
                            .SetPipeline(Pipeline)
                            .AddBindingSet(BindingSet)
                            .AddBindingSet(ShadowPassSet)
                            .SetIndirectParams(IndirectDrawBuffer);
                    
                        CmdList.SetGraphicsState(GraphicsState);

                        CmdList.SetPushConstants(&Cascade.LightViewProjection, sizeof(glm::mat4));
                        CmdList.DrawIndexedIndirect(1, Batch.IndirectDrawOffset * sizeof(FDrawIndexedIndirectArguments));
                    }
                }
            });
        }

        void FRenderScene::GBufferPass(FRenderGraph& RenderGraph, const FViewVolume& View)
        {
            RenderGraph.AddPass<RG_Raster>(FRGEvent("GBuffer Pass"), nullptr, [&](ICommandList& CmdList)
            {
                LUMINA_PROFILE_SECTION_COLORED("GBuffer Pass", tracy::Color::Red);
                
                if (MeshDrawCommands.empty())
                {
                    return;
                }

                FRenderPassDesc::FAttachment Position; Position
                    .SetImage(GBuffer.Position);
                
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
                    .SetLoadOp(ERenderLoadOp::Load)
                    .SetStoreOp(ERenderStoreOp::Store);

                
                FRenderPassDesc RenderPass; RenderPass
                    .AddColorAttachment(Position)
                    .AddColorAttachment(Normals)
                    .AddColorAttachment(Material)
                    .AddColorAttachment(AlbedoSpec)
                    .AddColorAttachment(PickerImageAttachment)
                    .SetDepthAttachment(Depth)
                    .SetRenderArea(GetRenderTarget()->GetExtent());
                

                FBlendState BlendState;
                FBlendState::RenderTarget PositionTarget;
                PositionTarget.SetFormat(EFormat::RGBA16_FLOAT);
                BlendState.SetRenderTarget(0, PositionTarget);
                
                FBlendState::RenderTarget NormalTarget;
                NormalTarget.SetFormat(EFormat::RGBA16_FLOAT);
                BlendState.SetRenderTarget(1, NormalTarget);
                
                FBlendState::RenderTarget MaterialTarget;
                MaterialTarget.SetFormat(EFormat::RGBA8_UNORM);
                BlendState.SetRenderTarget(2, MaterialTarget);
                
                FBlendState::RenderTarget AlbedoSpecTarget;
                AlbedoSpecTarget.SetFormat(EFormat::RGBA8_UNORM);
                BlendState.SetRenderTarget(3, AlbedoSpecTarget);

                FBlendState::RenderTarget PickerTarget;
                PickerTarget.SetFormat(EFormat::R32_UINT);
                BlendState.SetRenderTarget(4, PickerTarget);
                
                FRasterState RasterState;
                RasterState.EnableDepthClip();

                FDepthStencilState DepthState; DepthState
                    .SetDepthFunc(EComparisonFunc::GreaterOrEqual)
                    .DisableDepthWrite();
                
                FRenderState RenderState;
                RenderState.SetBlendState(BlendState);
                RenderState.SetRasterState(RasterState);
                RenderState.SetDepthStencilState(DepthState);
                
                for (SIZE_T CurrentDraw = 0; CurrentDraw < MeshDrawCommands.size(); ++CurrentDraw)
                {
                    const FMeshDrawCommand& Batch = MeshDrawCommands[CurrentDraw];
                    CMaterialInterface* Mat = Batch.Material;

                    FGraphicsPipelineDesc Desc; Desc
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
                        .SetViewport(MakeViewportStateFromImage(GetRenderTarget()))
                        .SetPipeline(GRenderContext->CreateGraphicsPipeline(Desc))
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
                RenderGraph.AddPass<RG_Raster>(FRGEvent("SSAO Pass"), nullptr, [&] (ICommandList& CmdList)
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
                        .SetRenderArea(GetRenderTarget()->GetExtent());

                    FRasterState RasterState;
                    RasterState.SetCullNone();
        
                    FBlendState BlendState;
                    FBlendState::RenderTarget RenderTarget;
                    RenderTarget.SetFormat(EFormat::R8_UNORM);
                    BlendState.SetRenderTarget(0, RenderTarget);
        
                    FDepthStencilState DepthState;
                    DepthState.DisableDepthTest();
                    DepthState.DisableDepthWrite();
                
                    FRenderState RenderState;
                    RenderState.SetRasterState(RasterState);
                    RenderState.SetDepthStencilState(DepthState);
                    RenderState.SetBlendState(BlendState);
                
                    FGraphicsPipelineDesc Desc;
                    Desc.SetRenderState(RenderState);
                    Desc.AddBindingLayout(BindingLayout);
                    Desc.AddBindingLayout(SSAOPassLayout);
                    Desc.SetVertexShader(VertexShader);
                    Desc.SetPixelShader(PixelShader);
        
                    FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc);

                    FGraphicsState GraphicsState;
                    GraphicsState.SetPipeline(Pipeline);
                    GraphicsState.AddBindingSet(BindingSet);
                    GraphicsState.AddBindingSet(SSAOPassSet);
                    GraphicsState.SetRenderPass(RenderPass);
                    GraphicsState.SetViewport(MakeViewportStateFromImage(GetRenderTarget()));

                    CmdList.SetGraphicsState(GraphicsState);
                    
                    CmdList.Draw(3, 1, 0, 0); 
                });


                RenderGraph.AddPass<RG_Raster>(FRGEvent("SSAO Blur Pass"), nullptr, [&](ICommandList& CmdList)
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
        
                    FBlendState BlendState;
                    FBlendState::RenderTarget RenderTarget;
                    RenderTarget.SetFormat(EFormat::R8_UNORM);
                    BlendState.SetRenderTarget(0, RenderTarget);
        
                    FDepthStencilState DepthState;
                    DepthState.DisableDepthTest();
                    DepthState.DisableDepthWrite();
                
                    FRenderState RenderState;
                    RenderState.SetRasterState(RasterState);
                    RenderState.SetDepthStencilState(DepthState);
                    RenderState.SetBlendState(BlendState);
                
                    FGraphicsPipelineDesc Desc;
                    Desc.SetRenderState(RenderState);
                    Desc.AddBindingLayout(BindingLayout);
                    Desc.AddBindingLayout(SSAOBlurPassLayout);
                    Desc.SetVertexShader(VertexShader);
                    Desc.SetPixelShader(PixelShader);
        
                    FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc);

                    FGraphicsState GraphicsState;
                    GraphicsState.SetPipeline(Pipeline);
                    GraphicsState.AddBindingSet(BindingSet);
                    GraphicsState.AddBindingSet(SSAOBlurPassSet);
                    GraphicsState.SetRenderPass(RenderPass);
                    GraphicsState.SetViewport(MakeViewportStateFromImage(GetRenderTarget()));

                    CmdList.SetGraphicsState(GraphicsState);
                    
                    CmdList.Draw(3, 1, 0, 0); 
                });
            }
            else
            {
                RenderGraph.AddPass<RG_Raster>(FRGEvent("SSAO Clear Pass"), nullptr, [&](ICommandList& CmdList)
                {
                    CmdList.ClearImageUInt(SSAOBlur, AllSubresources, 0.0f);
                });
            }
        }

        void FRenderScene::EnvironmentPass(FRenderGraph& RenderGraph)
        {
            if (RenderSettings.bHasEnvironment)
            {
                RenderGraph.AddPass<RG_Raster>(FRGEvent("Environment Pass"), nullptr, [&](ICommandList& CmdList)
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

                    FRenderPassDesc::FAttachment Depth; Depth
                        .SetImage(DepthAttachment)
                        .SetLoadOp(ERenderLoadOp::Load)
                        .SetStoreOp(ERenderStoreOp::Store);

                    FRenderPassDesc RenderPass; RenderPass
                        .AddColorAttachment(Attachment)
                        .SetDepthAttachment(Depth)
                        .SetRenderArea(GetRenderTarget()->GetExtent());
            
                    FRasterState RasterState;
                    RasterState.SetCullNone();
            
                    FBlendState BlendState;
                    FBlendState::RenderTarget RenderTarget;
                    BlendState.SetRenderTarget(0, RenderTarget);
            
                    FDepthStencilState DepthState;
                    DepthState.EnableDepthTest();
                    DepthState.SetDepthFunc(EComparisonFunc::GreaterOrEqual);
                    DepthState.DisableDepthWrite();
            
                    FRenderState RenderState;
                    RenderState.SetRasterState(RasterState);
                    RenderState.SetDepthStencilState(DepthState);
                    RenderState.SetBlendState(BlendState);
            
                    FGraphicsPipelineDesc Desc;
                    Desc.SetRenderState(RenderState);
                    Desc.AddBindingLayout(BindingLayout);
                    Desc.SetVertexShader(VertexShader);
                    Desc.SetPixelShader(PixelShader);
            
                    FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc);
            
                    FGraphicsState GraphicsState;
                    GraphicsState.AddBindingSet(BindingSet);
                    GraphicsState.SetPipeline(Pipeline);
                    GraphicsState.SetRenderPass(RenderPass);
                    GraphicsState.SetViewport(MakeViewportStateFromImage(GetRenderTarget()));
          
                    CmdList.SetGraphicsState(GraphicsState);
                
                    CmdList.Draw(3, 1, 0, 0); 
                });
            }
        }

        void FRenderScene::DeferredLightingPass(FRenderGraph& RenderGraph)
        {
            RenderGraph.AddPass<RG_Raster>(FRGEvent("Lighting Pass"), nullptr, [&](ICommandList& CmdList)
            {
                LUMINA_PROFILE_SECTION_COLORED("Lighting Pass", tracy::Color::Red2);
                
                FRHIVertexShaderRef VertexShader = FShaderLibrary::GetVertexShader("FullscreenQuad.vert");
                FRHIPixelShaderRef PixelShader = FShaderLibrary::GetPixelShader("DeferredLighting.frag");
                if (!VertexShader || !PixelShader)
                {
                    return;
                }

                FRenderPassDesc::FAttachment Attachment; Attachment
                    .SetLoadOp(ERenderLoadOp::Load)
                    .SetImage(GetRenderTarget());

                FRenderPassDesc::FAttachment Depth; Depth
                    .SetImage(DepthAttachment)
                    .SetLoadOp(ERenderLoadOp::Load)
                    .SetStoreOp(ERenderStoreOp::Store);

                FRenderPassDesc RenderPass; RenderPass
                    .AddColorAttachment(Attachment)
                    .SetDepthAttachment(Depth)
                    .SetRenderArea(GetRenderTarget()->GetExtent());
                
                FRasterState RasterState;
                RasterState.SetCullNone();
                
                FBlendState BlendState;
                FBlendState::RenderTarget RenderTarget;
                BlendState.SetRenderTarget(0, RenderTarget);
        
                FDepthStencilState DepthState;
                DepthState.EnableDepthTest();
                if (!RenderSettings.bHasEnvironment)
                {
                    DepthState.SetDepthFunc(EComparisonFunc::LessOrEqual);
                }
                DepthState.DisableDepthWrite();
                
                FRenderState RenderState;
                RenderState.SetRasterState(RasterState);
                RenderState.SetDepthStencilState(DepthState);
                RenderState.SetBlendState(BlendState);
                
                FGraphicsPipelineDesc Desc;
                Desc.SetRenderState(RenderState);
                Desc.AddBindingLayout(BindingLayout);
                Desc.AddBindingLayout(LightingPassLayout);
                Desc.SetVertexShader(VertexShader);
                Desc.SetPixelShader(PixelShader);
        
                FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc);

                FGraphicsState GraphicsState;
                GraphicsState.SetPipeline(Pipeline);
                GraphicsState.AddBindingSet(BindingSet);
                GraphicsState.AddBindingSet(LightingPassSet);
                GraphicsState.SetRenderPass(RenderPass);                
                GraphicsState.SetViewport(MakeViewportStateFromImage(GetRenderTarget()));

                CmdList.SetGraphicsState(GraphicsState);
                
                CmdList.Draw(3, 1, 0, 0); 
            });
        }

        void FRenderScene::BatchedLineDraw(FRenderGraph& RenderGraph)
        {
            RenderGraph.AddPass<RG_Raster>(FRGEvent("Batched Line Pass"), nullptr, [&](ICommandList& CmdList)
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
                    .SetLoadOp(ERenderLoadOp::Load)
                    .SetStoreOp(ERenderStoreOp::Store);

                FRenderPassDesc RenderPass; RenderPass
                    .AddColorAttachment(Attachment)
                    .SetDepthAttachment(Depth)
                    .SetRenderArea(GetRenderTarget()->GetExtent());

                
                FRasterState RasterState;
                RasterState.SetCullNone();
                RasterState.SetLineWidth(2.5f);
        
                FBlendState BlendState;
                FBlendState::RenderTarget RenderTarget;
                BlendState.SetRenderTarget(0, RenderTarget);
        
                FDepthStencilState DepthState;
                DepthState.EnableDepthTest();
                DepthState.SetDepthFunc(EComparisonFunc::GreaterOrEqual);
                DepthState.DisableDepthWrite();
                
                FRenderState RenderState;
                RenderState.SetRasterState(RasterState);
                RenderState.SetDepthStencilState(DepthState);
                RenderState.SetBlendState(BlendState);
                
                FGraphicsPipelineDesc Desc;
                Desc.SetPrimType(EPrimitiveType::LineList);
                Desc.SetInputLayout(SimpleVertexLayoutInput);
                Desc.SetRenderState(RenderState);
                Desc.AddBindingLayout(SimplePassLayout);
                Desc.SetVertexShader(VertexShader);
                Desc.SetPixelShader(PixelShader);
        
                FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc);

                FGraphicsState GraphicsState;
                GraphicsState.SetPipeline(Pipeline);
                GraphicsState.AddVertexBuffer({ SimpleVertexBuffer });
                GraphicsState.SetRenderPass(RenderPass);
                GraphicsState.SetViewport(MakeViewportStateFromImage(GetRenderTarget()));

                CmdList.SetGraphicsState(GraphicsState);

                SCameraComponent& CameraComponent = World->GetActiveCamera();
                glm::mat4 ViewProjMatrix = CameraComponent.GetViewProjectionMatrix();
                CmdList.SetPushConstants(&ViewProjMatrix, sizeof(glm::mat4));
                CmdList.Draw((uint32)SimpleVertices.size(), 1, 0, 0); 
            });
        }

        void FRenderScene::DebugDrawPass(FRenderGraph& RenderGraph)
        {
            RenderGraph.AddPass<RG_Raster>(FRGEvent("Debug Draw Pass"), nullptr, [&](ICommandList& CmdList)
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
                
                FBlendState BlendState;
                FBlendState::RenderTarget RenderTarget;
                RenderTarget.SetFormat(EFormat::RGBA32_FLOAT);
                BlendState.SetRenderTarget(0, RenderTarget);
        
                FDepthStencilState DepthState;
                DepthState.DisableDepthTest();
                DepthState.DisableDepthWrite();
                
                FRenderState RenderState;
                RenderState.SetRasterState(RasterState);
                RenderState.SetDepthStencilState(DepthState);
                RenderState.SetBlendState(BlendState);
                
                FGraphicsPipelineDesc Desc;
                Desc.SetRenderState(RenderState);
                Desc.AddBindingLayout(BindingLayout);
                Desc.AddBindingLayout(DebugPassLayout);
                Desc.SetVertexShader(VertexShader);
                Desc.SetPixelShader(PixelShader);
        
                FRHIGraphicsPipelineRef Pipeline = GRenderContext->CreateGraphicsPipeline(Desc);

                FGraphicsState GraphicsState;
                GraphicsState.SetPipeline(Pipeline);
                GraphicsState.AddBindingSet(BindingSet);
                GraphicsState.AddBindingSet(DebugPassSet);
                GraphicsState.SetRenderPass(RenderPass);               
                GraphicsState.SetViewport(MakeViewportStateFromImage(DebugVisualizationImage));

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
            ViewportState.Viewport = FViewport(SizeX, SizeY);
            ViewportState.Scissor = FRect(SizeX, SizeY);

            return ViewportState;
        }

        void FRenderScene::SetViewVolume(const FViewVolume& ViewVolume)
        {
            SceneViewport->SetViewVolume(ViewVolume);
            
            SceneGlobalData.CameraData.Location =           glm::vec4(SceneViewport->GetViewVolume().GetViewPosition(), 1.0f);
            SceneGlobalData.CameraData.View =               SceneViewport->GetViewVolume().GetViewMatrix();
            SceneGlobalData.CameraData.InverseView =        SceneViewport->GetViewVolume().GetInverseViewMatrix();
            SceneGlobalData.CameraData.Projection =         SceneViewport->GetViewVolume().GetProjectionMatrix();
            SceneGlobalData.CameraData.InverseProjection =  SceneViewport->GetViewVolume().GetInverseProjectionMatrix();
            SceneGlobalData.Time =                          (float)World->GetTimeSinceWorldCreation();
            SceneGlobalData.DeltaTime =                     (float)World->GetWorldDeltaTime();
            SceneGlobalData.FarPlane =                      SceneViewport->GetViewVolume().GetFar();
            SceneGlobalData.NearPlane =                     SceneViewport->GetViewVolume().GetNear();


        }

        void FRenderScene::ResetPass(FRenderGraph& RenderGraph)
        {
            SimpleVertices.clear();
            MeshDrawCommands.clear();
            IndirectDrawArguments.clear();
            InstanceData.clear();
            LightData.NumLights = 0;

            RenderGraph.AddPass<RG_Raster>(FRGEvent("Clear Overdraw"), nullptr, [&](ICommandList& CmdList)
            {
               CmdList.ClearImageUInt(OverdrawImage, AllSubresources, 0); 
            });
            
        }

        void FRenderScene::CompileDrawCommands()
        {
            LUMINA_PROFILE_SCOPE();

            ICommandList* CommandList = GRenderContext->GetCommandList(ECommandQueue::Graphics);
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
                        const SIZE_T NumSurfaces = Resource.GetNumSurfaces();
                        
                        const uintptr_t MeshPtr = reinterpret_cast<uintptr_t>(Mesh);
                        const uint64 MeshID = (MeshPtr & 0xFFFFFull) << 24;

                        glm::mat4 TransformMatrix = TransformComponent.GetMatrix();
                        FAABB BoundingBox = Mesh->GetAABB().ToWorld(TransformMatrix);
                        glm::vec3 Center = (BoundingBox.Min + BoundingBox.Max) * 0.5f;
                        glm::vec3 Extents = BoundingBox.Max - Center;
                        float Radius = glm::length(Extents);
                        glm::vec4 SphereBounds = glm::vec4(Center, Radius);

                        for (const FGeometrySurface& Surface : Resource.GeometrySurfaces)
                        {
                            CMaterialInterface* Material = MeshComponent.GetMaterialForSlot(Surface.MaterialIndex);
                            
                            if (!IsValid(Material) || !Material->IsReadyForRender())
                            {
                                continue;
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
                for (FDrawIndexedIndirectArguments& Arg : IndirectDrawArguments)
                {
                    Arg.InstanceCount = 0;
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
                auto Group = World->GetEntityRegistry().group<SPointLightComponent>(entt::get<STransformComponent>);
                for (uint64 i = 0; i < Group.size(); ++i)
                {
                    entt::entity entity = Group[i];
                    const SPointLightComponent& PointLightComponent = Group.get<SPointLightComponent>(entity);
                    const STransformComponent& TransformComponent = Group.get<STransformComponent>(entity);
        
                    FLight Light;
                    Light.Type = LIGHT_TYPE_POINT;
                    
                    Light.Color = glm::vec4(PointLightComponent.LightColor, PointLightComponent.Intensity);
                    Light.Radius = PointLightComponent.Attenuation;
                    Light.Position = glm::vec4(TransformComponent.WorldTransform.Location, 1.0f);
                    
                    LightData.Lights[LightData.NumLights++] = Memory::Move(Light);
        
                    //Scene->DrawDebugSphere(Transform.Location, 0.25f, Light.Color);
                }
            }
        
            //========================================================================================================================
        
        
            {
                auto Group = World->GetEntityRegistry().group<>(entt::get<SCameraComponent>, entt::exclude<SEditorComponent>);
                for (uint64 i = 0; i < Group.size(); ++i)
                {
                    entt::entity entity = Group[i];
                    SCameraComponent& CameraComponent = Group.get<SCameraComponent>(entity);
                    World->DrawArrow(CameraComponent.GetPosition(), CameraComponent.GetForwardVector(), 1.0f, FColor::Blue);
                    World->DrawFrustum(CameraComponent.GetViewVolume().GetViewProjectionMatrix(), FColor::Green);
                }
            }
        
            //========================================================================================================================
            
            {
                auto Group = World->GetEntityRegistry().group<SSpotLightComponent>(entt::get<STransformComponent>);
                for (uint64 i = 0; i < Group.size(); ++i)
                {
                    entt::entity entity = Group[i];
                    const SSpotLightComponent& SpotLightComponent = Group.get<SSpotLightComponent>(entity);
                    const FTransform& Transform = Group.get<STransformComponent>(entity).WorldTransform;
        
                    FLight SpotLight;
                    SpotLight.Type = LIGHT_TYPE_SPOT;
                    
                    SpotLight.Position = glm::vec4(Transform.Location, 1.0f);
        
                    glm::vec3 Forward = Transform.Rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                    SpotLight.Direction = glm::vec4(glm::normalize(Forward), 0.0f);
                    
                    SpotLight.Color = glm::vec4(SpotLightComponent.LightColor, SpotLightComponent.Intensity);
        
                    float InnerDegrees = SpotLightComponent.InnerConeAngle;
                    float OuterDegrees = SpotLightComponent.OuterConeAngle;
        
                    float InnerCos = glm::cos(glm::radians(InnerDegrees));
                    float OuterCos = glm::cos(glm::radians(OuterDegrees));
                    
                    SpotLight.Angle = glm::vec2(InnerCos, OuterCos);
        
                    SpotLight.Radius = SpotLightComponent.Attenuation;
        
                    LightData.Lights[LightData.NumLights++] = SpotLight;
        
                    //Scene->DrawDebugCone(SpotLight.Position, Forward, glm::radians(OuterDegrees), SpotLightComponent.Attenuation, glm::vec4(SpotLightComponent.LightColor, 1.0f));
                    //Scene->DrawDebugCone(SpotLight.Position, Forward, glm::radians(InnerDegrees), SpotLightComponent.Attenuation, glm::vec4(SpotLightComponent.LightColor, 1.0f));
        
                }
            }
        
            
            //========================================================================================================================
        
        
            {
                bHasDirectionalLight = false;
            
                auto View = World->GetEntityRegistry().view<SDirectionalLightComponent>();
                View.each([this](const SDirectionalLightComponent& DirectionalLightComponent)
                {
                    bHasDirectionalLight = true;
            
                    // Setup light data
                    FLight DirectionalLight;
                    DirectionalLight.Type = LIGHT_TYPE_DIRECTIONAL;
                    DirectionalLight.Color = glm::vec4(DirectionalLightComponent.Color, DirectionalLightComponent.Intensity);
                    DirectionalLight.Direction = glm::vec4(glm::normalize(DirectionalLightComponent.Direction), 1.0f);
            
                    SceneGlobalData.SunDirection = DirectionalLight.Direction;
                    LightData.Lights[LightData.NumLights++] = DirectionalLight;

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
                        SceneGlobalData.CascadeSplits[i] = SplitDist;
                        
                        FShadowCascade& Cascade = ShadowCascades[i];
                        Cascade.DirectionalLight = DirectionalLight;
            
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

                        glm::vec3 LightDir = -DirectionalLight.Direction;
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

                        SceneGlobalData.LightViewProj[i] = Cascade.LightViewProjection;
                        
                        Cascade.ShadowMapSize = glm::ivec2(GShadowMapResolution);

                        LastSplitDist = CascadeSplits[i];
                    }
                });
            }

            
            //========================================================================================================================
        
            {
                RenderSettings.bHasEnvironment = false;
                SceneGlobalData.AmbientLight = glm::vec4(0.0f);
                RenderSettings.bSSAO = false;
                auto View = World->GetEntityRegistry().view<SEnvironmentComponent>();
                View.each([this] (const SEnvironmentComponent& EnvironmentComponent)
                {
                    SceneGlobalData.AmbientLight                        = glm::vec4(EnvironmentComponent.AmbientLight.Color, EnvironmentComponent.AmbientLight.Intensity);
                    RenderSettings.bHasEnvironment                      = true;
                    RenderSettings.bSSAO                                = EnvironmentComponent.bSSAOEnabled;
                    SceneGlobalData.SSAOSettings.Intensity              = EnvironmentComponent.SSAOInfo.Intensity;
                    SceneGlobalData.SSAOSettings.Power                  = EnvironmentComponent.SSAOInfo.Power;
                    SceneGlobalData.SSAOSettings.Radius                 = EnvironmentComponent.SSAOInfo.Radius;
                });
            }

            {
                LUMINA_PROFILE_SECTION("Write Buffers");

                const size_t SimpleVertexSize = SimpleVertices.size() * sizeof(FSimpleElementVertex);
                const size_t InstanceDataSize = InstanceData.size() * sizeof(FInstanceData);
                const size_t IndirectArgsSize = IndirectDrawArguments.size() * sizeof(FDrawIndexedIndirectArguments);
                SIZE_T LightUploadSize = sizeof(FSceneLightData::NumLights) + sizeof(FSceneLightData::Padding) + sizeof(FLight) * LightData.NumLights;

                CommandList->SetBufferState(SceneDataBuffer, EResourceStates::CopyDest);
                CommandList->SetBufferState(InstanceDataBuffer, EResourceStates::CopyDest);
                CommandList->SetBufferState(IndirectDrawBuffer, EResourceStates::CopyDest);
                CommandList->SetBufferState(SimpleVertexBuffer, EResourceStates::CopyDest);
                CommandList->SetBufferState(LightDataBuffer, EResourceStates::CopyDest);
                CommandList->CommitBarriers();

                CommandList->DisableAutomaticBarriers();
                CommandList->WriteBuffer(SceneDataBuffer, &SceneGlobalData, 0, sizeof(FSceneGlobalData));
                CommandList->WriteBuffer(InstanceDataBuffer, InstanceData.data(), 0, InstanceDataSize);
                CommandList->WriteBuffer(IndirectDrawBuffer, IndirectDrawArguments.data(), 0, IndirectArgsSize);
                CommandList->WriteBuffer(SimpleVertexBuffer, SimpleVertices.data(), 0, SimpleVertexSize);
                CommandList->WriteBuffer(LightDataBuffer, &LightData, 0, LightUploadSize);
                CommandList->EnableAutomaticBarriers();
            }
        }
        

        void FRenderScene::InitResources()
        {
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
                SetDesc.AddItem(FBindingSetItem::TextureSRV(0, GBuffer.Position));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(1, GBuffer.Normals));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(2, GBuffer.Material));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(3, GBuffer.AlbedoSpec));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(4, SSAOBlur));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(5, ShadowCascades[0].ShadowMapImage));

                TBitFlags<ERHIShaderType> Visibility;
                Visibility.SetMultipleFlags(ERHIShaderType::Fragment);
                GRenderContext->CreateBindingSetAndLayout(Visibility, 0, SetDesc, LightingPassLayout, LightingPassSet);

            }

            {
                FBindingSetDesc SetDesc;
                SetDesc.AddItem(FBindingSetItem::TextureSRV(0, GetRenderTarget()));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(1, GBuffer.Position));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(2, DepthAttachment));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(3, GBuffer.Normals));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(4, GBuffer.AlbedoSpec));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(5, SSAOBlur));
                SetDesc.AddItem(FBindingSetItem::TextureSRV(6, ShadowCascades[0].ShadowMapImage));

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
                SetDesc.AddItem(FBindingSetItem::PushConstants(0, sizeof(glm::mat4)));

                TBitFlags<ERHIShaderType> Visibility;
                Visibility.SetMultipleFlags(ERHIShaderType::Vertex);
                GRenderContext->CreateBindingSetAndLayout(Visibility, 0, SetDesc, ShadowPassLayout, ShadowPassSet);

            }

            {
                FBindingSetDesc SetDesc;
                SetDesc.AddItem(FBindingSetItem::TextureSRV(0, GBuffer.Position));
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
                BufferDesc.Usage.SetMultipleFlags(BUF_UniformBuffer, BUF_Dynamic);
                BufferDesc.MaxVersions = 3;
                BufferDesc.DebugName = "Scene Global Data";
                SceneDataBuffer = GRenderContext->CreateBuffer(BufferDesc);
            }

            {
                FRHIBufferDesc BufferDesc;
                BufferDesc.Size = sizeof(FInstanceData) * 100'000;
                BufferDesc.Stride = sizeof(FInstanceData);
                BufferDesc.Usage.SetMultipleFlags(BUF_StorageBuffer);
                BufferDesc.bKeepInitialState = true;
                BufferDesc.InitialState = EResourceStates::ShaderResource;
                BufferDesc.DebugName = "Instance Buffer";
                InstanceDataBuffer = GRenderContext->CreateBuffer(BufferDesc);
            }

            {
                FRHIBufferDesc BufferDesc;
                BufferDesc.Size = sizeof(uint32) * 100'000;
                BufferDesc.Stride = sizeof(uint32);
                BufferDesc.Usage.SetMultipleFlags(BUF_StorageBuffer);
                BufferDesc.bKeepInitialState = true;
                BufferDesc.InitialState = EResourceStates::UnorderedAccess;
                BufferDesc.DebugName = "Instance Mapping";
                InstanceMappingBuffer = GRenderContext->CreateBuffer(BufferDesc);

                
                BufferDesc.Usage.SetFlag(EBufferUsageFlags::CPUReadable);
                BufferDesc.DebugName = "Instance Readback";
                InstanceReadbackBuffer = GRenderContext->CreateBuffer(BufferDesc);

            }

            {
                FRHIBufferDesc LightBufferDesc;
                LightBufferDesc.Size = sizeof(FSceneLightData);
                LightBufferDesc.Stride = sizeof(FSceneLightData);
                LightBufferDesc.Usage.SetMultipleFlags(BUF_StorageBuffer);
                LightBufferDesc.bKeepInitialState = true;
                LightBufferDesc.InitialState = EResourceStates::ShaderResource;
                LightBufferDesc.DebugName = "Light Data Buffer";
                LightDataBuffer = GRenderContext->CreateBuffer(LightBufferDesc);
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

                BufferDesc.Usage.SetFlag(EBufferUsageFlags::CPUReadable);
                BufferDesc.DebugName = "Instance Readback";
                InstanceReadbackBuffer = GRenderContext->CreateBuffer(BufferDesc);
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
                FRHIImageDesc ImageDesc;
                ImageDesc.Extent = Extent;
                ImageDesc.Flags.SetMultipleFlags(EImageCreateFlags::ColorAttachment, EImageCreateFlags::ShaderResource);
                ImageDesc.Format = EFormat::RGBA16_FLOAT;
                ImageDesc.Dimension = EImageDimension::Texture2D;
                ImageDesc.DebugName = "GBuffer - Position";
                ImageDesc.bKeepInitialState = true;
                ImageDesc.InitialState = EResourceStates::ShaderResource;
            
                GBuffer.Position = GRenderContext->CreateImage(ImageDesc);
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
                FRHIImageRef CascadeShadowMap;
                FRHIImageDesc ImageDesc = {};
                ImageDesc.Extent = FIntVector2D(4096, 4096);
                ImageDesc.Format = EFormat::D32;
                ImageDesc.Dimension = EImageDimension::Texture2DArray;
                ImageDesc.InitialState = EResourceStates::DepthWrite;
                ImageDesc.bKeepInitialState = true;
                ImageDesc.ArraySize = NumCascades;
                ImageDesc.Flags.SetMultipleFlags(EImageCreateFlags::DepthAttachment, EImageCreateFlags::ShaderResource);
                ImageDesc.DebugName = "ShadowCascadeMap";
                
                CascadeShadowMap = GRenderContext->CreateImage(ImageDesc);
                
                for (int i = 0; i < NumCascades; ++i)
                {
                    FShadowCascade& Cascade = ShadowCascades[i];
                    Cascade.ShadowMapImage = CascadeShadowMap;
                    Cascade.ShadowMapSize = glm::ivec2(4096);
                }
            }
            
            //==================================================================================================

            {
                FRHIImageDesc ImageDesc;
                ImageDesc.Extent = {2048, 2048};
                ImageDesc.Flags.SetFlag(EImageCreateFlags::CubeCompatible);
                ImageDesc.Flags.SetFlag(EImageCreateFlags::ShaderResource);
                ImageDesc.Format = EFormat::RGBA8_UNORM;
                ImageDesc.Dimension = EImageDimension::Texture2D;
                ImageDesc.ArraySize = 6;
                ImageDesc.DebugName = "Skybox CubeMap";

                CubeMap = GRenderContext->CreateImage(ImageDesc);

                static const char* CubeFaceFiles[6] = {
                    "right.jpg", "left.jpg", "top.jpg", "bottom.jpg", "front.jpg", "back.jpg"
                };

                FRHICommandListRef CommandList = GRenderContext->CreateCommandList(FCommandListInfo::Transfer());
                CommandList->Open();

                CommandList->BeginTrackingImageState(CubeMap, AllSubresources, EResourceStates::Common);
            
                for (int i = 0; i < 6; ++i)
                {
                    FString ResourceDirectory = Paths::GetEngineResourceDirectory();
                    ResourceDirectory += FString("/Textures/CubeMaps/Mountains/") + CubeFaceFiles[i];
                
                    TVector<uint8> Pixels;
                    FIntVector2D ImageExtent = Import::Textures::ImportTexture(Pixels, ResourceDirectory, false);
                
                    const uint32 width = ImageExtent.X;
                    const uint32 height = ImageExtent.Y;
                    const SIZE_T rowPitch = width * 4;  // 4 bytes per pixel (RGBA8)
                    const SIZE_T depthPitch = rowPitch * height;
                
                    CommandList->WriteImage(CubeMap, i, 0, Pixels.data(), rowPitch, depthPitch);
                }

                CommandList->SetPermanentImageState(CubeMap, EResourceStates::ShaderResource);
                CommandList->CommitBarriers();
            
                CommandList->Close();
                GRenderContext->ExecuteCommandList(CommandList, ECommandQueue::Transfer);
            }

            //==================================================================================================

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
                FRHIImageDesc ImageDesc;
                ImageDesc.Extent = {1024, 1024};
                ImageDesc.Flags.SetFlag(EImageCreateFlags::CubeCompatible);
                ImageDesc.Flags.SetFlag(EImageCreateFlags::ShaderResource);
                ImageDesc.Format = EFormat::RGBA8_UNORM;
                ImageDesc.bKeepInitialState = true;
                ImageDesc.InitialState = EResourceStates::ShaderResource;
                ImageDesc.Dimension = EImageDimension::Texture2D;
                ImageDesc.ArraySize = 6;
                ImageDesc.DebugName = "Shadow Cubemap";

                ShadowCubeMap = GRenderContext->CreateImage(ImageDesc);
            }
        }
    }
