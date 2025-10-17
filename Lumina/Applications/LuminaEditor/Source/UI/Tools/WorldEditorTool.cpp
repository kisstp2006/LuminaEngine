#include "WorldEditorTool.h"

#include "glm/gtc/type_ptr.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "EditorToolContext.h"
#include "Assets/AssetRegistry/AssetRegistry.h"
#include "Core/Object/ObjectIterator.h"
#include "Core/Object/Package/Package.h"
#include "glm/gtx/matrix_decompose.hpp"
#include "Paths/Paths.h"
#include "Tools/UI/ImGui/ImGuiX.h"
#include "World/WorldManager.h"
#include "World/Entity/Components/CameraComponent.h"
#include "World/Entity/Components/DirtyComponent.h"
#include "World/Entity/Components/EditorComponent.h"
#include "World/Entity/Components/NameComponent.h"
#include "World/Entity/Components/RelationshipComponent.h"
#include "World/Entity/Components/VelocityComponent.h"
#include "World/Entity/Systems/EditorEntityMovementSystem.h"
#include "World/Scene/RenderScene/RenderScene.h"
#include "World/Scene/RenderScene/SceneRenderTypes.h"


namespace Lumina
{
    static constexpr const char* SystemOutlinerName = "Systems";
    
    FWorldEditorTool::FWorldEditorTool(IEditorToolContext* Context, CWorld* InWorld)
        : FEditorTool(Context, InWorld->GetName().ToString(), InWorld)
        , SelectedSystem(nullptr)
        , SelectedEntity()
        , CopiedEntity()
        , OutlinerContext()
        , SystemsContext()
    {
        GuizmoOp = ImGuizmo::TRANSLATE;
        GuizmoMode = ImGuizmo::WORLD;
    }

    void FWorldEditorTool::OnInitialize()
    {
        CreateToolWindow("Outliner", [this] (const FUpdateContext& Context, bool bisFocused)
        {
            DrawOutliner(Context, bisFocused);
        });

        CreateToolWindow(SystemOutlinerName, [this] (const FUpdateContext& Context, bool bisFocused)
        {
            DrawSystems(Context, bisFocused);
        });
        
        CreateToolWindow("Details", [this] (const FUpdateContext& Context, bool bisFocused)
        {
            DrawObjectEditor(Context, bisFocused);
        });


        //------------------------------------------------------------------------------------------------------

        OutlinerContext.DrawItemContextMenuFunction = [this](const TVector<FTreeListViewItem*>& Items)
        {
            for (FTreeListViewItem* Item : Items)
            {
                FEntityListViewItem* EntityListItem = static_cast<FEntityListViewItem*>(Item);
                if (EntityListItem->GetEntity().HasComponent<SEditorComponent>())
                {
                    continue;
                }
                
                if (ImGui::MenuItem("Add Component"))
                {
                    PushAddComponentModal(EntityListItem->GetEntity());
                }
                
                if (ImGui::MenuItem("Rename"))
                {
                    PushRenameEntityModal(EntityListItem->GetEntity());
                }

                if (ImGui::MenuItem("Duplicate"))
                {
                    Entity New;
                    CopyEntity(New, SelectedEntity);
                }
                
                if (ImGui::MenuItem("Delete"))
                {
                    EntityDestroyRequests.push(EntityListItem->GetEntity());
                }
            }
        };

        OutlinerContext.RebuildTreeFunction = [this](FTreeListView* Tree)
        {
            RebuildSceneOutliner(Tree);
        };
        
        OutlinerContext.ItemSelectedFunction = [this](FTreeListViewItem* Item)
        {
            if (Item == nullptr)
            {
                SelectedEntity = Entity();
                return;
            }
            
            FEntityListViewItem* EntityListItem = static_cast<FEntityListViewItem*>(Item);
            
            SelectedSystem = nullptr;
            SelectedEntity = EntityListItem->GetEntity();
            
            RebuildPropertyTables();
        };

        OutlinerContext.DragDropFunction = [this](FTreeListViewItem* Item)
        {
            HandleEntityEditorDragDrop(Item);  
        };

        OutlinerContext.FilterFunc = [this](const FTreeListViewItem* Item) -> bool
        {
            return EntityFilterState.FilterName.PassFilter(Item->GetName().c_str());
        };
        
        //------------------------------------------------------------------------------------------------------


        SystemsContext.RebuildTreeFunction = [this](FTreeListView* Tree)
        {
            for (uint8 i = 0; i < (uint8)EUpdateStage::Max; ++i)
            {
                for (CEntitySystem* System : World->GetSystemsForUpdateStage((EUpdateStage)i))
                {
                    SystemsListView.AddItemToTree<FSystemListViewItem>(nullptr, eastl::move(System));
                }
            }
        };

        SystemsContext.ItemSelectedFunction = [this](FTreeListViewItem* Item)
        {
            FSystemListViewItem* ListItem = static_cast<FSystemListViewItem*>(Item);
            SelectedSystem = ListItem->GetSystem();
            SelectedEntity = {};
            RebuildPropertyTables();
        };

        OutlinerListView.MarkTreeDirty();
        SystemsListView.MarkTreeDirty();
    }

    void FWorldEditorTool::OnDeinitialize(const FUpdateContext& UpdateContext)
    {
    }

    void FWorldEditorTool::OnSave()
    {
        FString FullPath = Paths::ResolveVirtualPath(World->GetPathName());
        Paths::AddPackageExtension(FullPath);

        if (CPackage::SavePackage(World->GetPackage(), FullPath.c_str()))
        {
            GetEngineSystem<FAssetRegistry>().AssetSaved(World);
            ImGuiX::Notifications::NotifySuccess("Successfully saved package: \"%s\"", World->GetPathName().c_str());
        }
        else
        {
            ImGuiX::Notifications::NotifyError("Failed to save package: \"%s\"", World->GetPathName().c_str());
        }
    }

    void FWorldEditorTool::Update(const FUpdateContext& UpdateContext)
    {
        while (!EntityDestroyRequests.empty())
        {
            Entity Entity = EntityDestroyRequests.back();
            EntityDestroyRequests.pop();

            if (Entity == SelectedEntity)
            {
                if (CopiedEntity == SelectedEntity)
                {
                    CopiedEntity = {};
                }
                
                SelectedEntity = {};
            }
            
            World->DestroyEntity(Entity);
            
            OutlinerListView.MarkTreeDirty();

        }

        if (SelectedEntity.IsValid())
        {
            
            SelectedEntity.EmplaceOrReplace<FNeedsTransformUpdate>();

            if (bViewportFocused)
            {
                if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_C))
                {
                    CopiedEntity = SelectedEntity;
                }

                if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_D))
                {
                    Entity New;
                    CopyEntity(New, SelectedEntity);
                }
            }
        }

        if (CopiedEntity && bViewportFocused)
        {
            if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_V))
            {
                Entity New;
                CopyEntity(New, CopiedEntity);
            }
        }

        if (SelectedEntity && bViewportFocused)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Delete))
            {
                EntityDestroyRequests.push(SelectedEntity);
            }
        }
    }

    void FWorldEditorTool::DrawToolMenu(const FUpdateContext& UpdateContext)
    {
        if (ImGui::BeginMenu(LE_ICON_CAMERA_CONTROL" Camera"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Movement Settings");
            ImGui::Separator();
            ImGui::Spacing();
            
            SVelocityComponent& VelocityComponent = EditorEntity.GetComponent<SVelocityComponent>();
            ImGui::SetNextItemWidth(200);
            ImGui::SliderFloat("##CameraSpeed", &VelocityComponent.Speed, 1.0f, 200.0f, "%.1f units/s");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Camera movement speed");
            }

            ImGui::PopStyleVar();
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu(LE_ICON_MOVE_RESIZE" Gizmo"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            
            static int currentOpIndex = 0;
            static int currentModeIndex = 0;

            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.4f, 1.0f), "Transform Operation");
            ImGui::Separator();

            const char* operations[] = { "Translate", "Rotate", "Scale" };
            constexpr int operationsCount = IM_ARRAYSIZE(operations);

            switch (GuizmoOp)
            {
            case ImGuizmo::TRANSLATE: currentOpIndex = 0; break;
            case ImGuizmo::ROTATE:    currentOpIndex = 1; break;
            case ImGuizmo::SCALE:     currentOpIndex = 2; break;
            default:                  currentOpIndex = 0; break;
            }

            ImGui::SetNextItemWidth(180);
            if (ImGui::Combo("##Operation", &currentOpIndex, operations, operationsCount))
            {
                switch (currentOpIndex)
                {
                case 0: GuizmoOp = ImGuizmo::TRANSLATE; break;
                case 1: GuizmoOp = ImGuizmo::ROTATE;    break;
                case 2: GuizmoOp = ImGuizmo::SCALE;     break;
                }
            }
            

            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.4f, 1.0f), "Transform Space");
            ImGui::Separator();

            const char* modes[] = { "World Space", "Local Space" };
            constexpr int modesCount = IM_ARRAYSIZE(modes);

            switch (GuizmoMode)
            {
            case ImGuizmo::WORLD: currentModeIndex = 0; break;
            case ImGuizmo::LOCAL: currentModeIndex = 1; break;
            default:              currentModeIndex = 0; break;
            }

            ImGui::SetNextItemWidth(180);
            if (ImGui::Combo("##Mode", &currentModeIndex, modes, modesCount))
            {
                switch (currentModeIndex)
                {
                case 0: GuizmoMode = ImGuizmo::WORLD; break;
                case 1: GuizmoMode = ImGuizmo::LOCAL; break;
                }
            }

            ImGui::PopStyleVar();
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu(LE_ICON_DEBUG_STEP_INTO" Renderer"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            
            FRenderScene* SceneRenderer = World->GetRenderer();
            FRHICommandListRef CommandList = GRenderContext->GetCommandList(ECommandQueue::Graphics);

            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "Performance Statistics (Global)");
            ImGui::Separator();
            
            ImGui::BeginTable("##StatsTable", 2, ImGuiTableFlags_None);
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Draw Calls");
            ImGui::TableNextColumn();
            ImGui::Text("%u", CommandList->GetCommandListStats().NumDrawCalls);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Dispatch Calls");
            ImGui::TableNextColumn();
            ImGui::Text("%u", CommandList->GetCommandListStats().NumDispatchCalls);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Buffer Writes");
            ImGui::TableNextColumn();
            ImGui::Text("%u", CommandList->GetCommandListStats().NumBufferWrites);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Copy Calls");
            ImGui::TableNextColumn();
            ImGui::Text("%u", CommandList->GetCommandListStats().NumCopies);
            
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Blit Calls");
            ImGui::TableNextColumn();
            ImGui::Text("%u", CommandList->GetCommandListStats().NumBlitCommands);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Clear Calls");
            ImGui::TableNextColumn();
            ImGui::Text("%u", CommandList->GetCommandListStats().NumClearCommands);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Num Bindings");
            ImGui::TableNextColumn();
            ImGui::Text("%u", CommandList->GetCommandListStats().NumBindings);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Num Push Constants");
            ImGui::TableNextColumn();
            ImGui::Text("%u", CommandList->GetCommandListStats().NumPushConstants);
            
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Pipeline Barriers");
            ImGui::TableNextColumn();
            ImGui::Text("%u", CommandList->GetCommandListStats().NumBarriers);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Pipeline Switches");
            ImGui::TableNextColumn();
            ImGui::Text("%u", CommandList->GetCommandListStats().NumPipelineSwitches);
            
            
            ImGui::EndTable();
    
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.8f, 1.0f), "Debug Visualization");
            ImGui::Separator();

            static const char* DebugLabels[] =
            {
                "None",
                "Position Buffer",
                "Normal Vectors",
                "Albedo Color",
                "SSAO Map",
                "Material Properties",
                "Depth Buffer",
                "Overdraw Heatmap",
                "Shadow Cascade 1",
                "Shadow Cascade 2",
                "Shadow Cascade 3",
                "Shadow Cascade 4",
            };

            ERenderSceneDebugFlags DebugMode = SceneRenderer->GetDebugMode();
            int DebugModeInt = static_cast<int>(DebugMode);
            
            ImGui::SetNextItemWidth(220);
            if (ImGui::Combo("##DebugVis", &DebugModeInt, DebugLabels, IM_ARRAYSIZE(DebugLabels)))
            {
                SceneRenderer->SetDebugMode(static_cast<ERenderSceneDebugFlags>(DebugModeInt));
            }

            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.8f, 1.0f), "Render Options");
            ImGui::Separator();

            bool bDrawAABB = (bool)SceneRenderer->GetSceneRenderSettings().bDrawAABB;
            if (ImGui::Checkbox("Show Bounding Boxes", &bDrawAABB))
            {
                SceneRenderer->GetSceneRenderSettings().bDrawAABB = bDrawAABB;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Visualize axis-aligned bounding boxes");
            }

            bool bUseInstancing = (bool)SceneRenderer->GetSceneRenderSettings().bUseInstancing;
            if (ImGui::Checkbox("GPU Instancing", &bUseInstancing))
            {
                SceneRenderer->GetSceneRenderSettings().bUseInstancing = bUseInstancing;
            }

            ImGui::SliderFloat("Cascade Split Lambda", &SceneRenderer->GetSceneRenderSettings().CascadeSplitLambda, 0.0f, 1.0f);
            
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Enable hardware instancing for repeated meshes");
            }

            ImGui::PopStyleVar();
            ImGui::EndMenu();
        }
    }

    void FWorldEditorTool::InitializeDockingLayout(ImGuiID InDockspaceID, const ImVec2& InDockspaceSize) const
    {
        ImGuiID dockLeft = 0;
        ImGuiID dockRight = 0;

        // 1. Split root dock vertically: left = viewport, right = other panels
        ImGui::DockBuilderSplitNode(InDockspaceID, ImGuiDir_Right, 0.25f, &dockRight, &dockLeft);

        ImGuiID dockRightTop = 0;
        ImGuiID dockRightBottom = 0;

        // 2. Split right dock horizontally into Outliner (top 25%) and bottom (Details + SystemOutliner)
        ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.25f, &dockRightTop, &dockRightBottom);

        ImGuiID dockRightBottomLeft = 0;
        ImGuiID dockRightBottomRight = 0;

        // 3. Split bottom right dock horizontally into Details (left) and SystemOutliner (right)
        ImGui::DockBuilderSplitNode(dockRightBottom, ImGuiDir_Right, 0.5f, &dockRightBottomRight, &dockRightBottomLeft);

        ImGui::DockBuilderDockWindow(GetToolWindowName(ViewportWindowName).c_str(), dockLeft);
        ImGui::DockBuilderDockWindow(GetToolWindowName("Outliner").c_str(), dockRightTop);
        ImGui::DockBuilderDockWindow(GetToolWindowName("Details").c_str(), dockRightBottomLeft);
        ImGui::DockBuilderDockWindow(GetToolWindowName(SystemOutlinerName).c_str(), dockRightBottomRight);
    }

    void FWorldEditorTool::DrawViewportOverlayElements(const FUpdateContext& UpdateContext, ImTextureRef ViewportTexture, ImVec2 ViewportSize)
    {
        if (bViewportHovered)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Space))
            {
                CycleGuizmoOp();
            }
        }
        
    
        SCameraComponent& CameraComponent = EditorEntity.GetComponent<SCameraComponent>();
    
        glm::mat4 ViewMatrix = CameraComponent.GetViewMatrix();
        glm::mat4 ProjectionMatrix = CameraComponent.GetProjectionMatrix();
        ProjectionMatrix[1][1] *= -1.0f;

        ImGuizmo::SetDrawlist(ImGui::GetCurrentWindow()->DrawList);
        ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ViewportSize.x, ViewportSize.y);

        if (SelectedEntity.IsValid() && CameraComponent.GetViewVolume().GetFrustum().IsInside(SelectedEntity.GetWorldTransform().Location))
        {
            glm::mat4 EntityMatrix = SelectedEntity.GetWorldMatrix();
            ImGuizmo::Manipulate(glm::value_ptr(ViewMatrix), glm::value_ptr(ProjectionMatrix), GuizmoOp, GuizmoMode, glm::value_ptr(EntityMatrix));
            
            if (ImGuizmo::IsUsing())
            {
                bImGuizmoUsedOnce = true;
                
                glm::mat4 WorldMatrix = EntityMatrix;
                glm::vec3 Translation, Scale, Skew;
                glm::quat Rotation;
                glm::vec4 Perspective;

                if (SelectedEntity.IsChild())
                {
                    glm::mat4 ParentWorldMatrix = SelectedEntity.GetParent().GetWorldTransform().GetMatrix();
                    glm::mat4 ParentWorldInverse = glm::inverse(ParentWorldMatrix);
                    glm::mat4 LocalMatrix = ParentWorldInverse * WorldMatrix;
                
                    glm::decompose(LocalMatrix, Scale, Rotation, Translation, Skew, Perspective);
                }
                else
                {
                    glm::decompose(WorldMatrix, Scale, Rotation, Translation, Skew, Perspective);
                }
        
                STransformComponent& TransformComponent = SelectedEntity.GetComponent<STransformComponent>();
        
                if (Translation != TransformComponent.GetLocation() || Rotation != TransformComponent.GetRotation() || Scale != TransformComponent.GetScale())
                {
                    TransformComponent.SetLocation(Translation).SetRotation(Rotation).SetScale(Scale);
                }
            }
        }

        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
        {
            uint32 PickerWidth = World->GetRenderer()->GetRenderTarget()->GetExtent().X;
            uint32 PickerHeight = World->GetRenderer()->GetRenderTarget()->GetExtent().Y;
            
            ImVec2 viewportScreenPos = ImGui::GetWindowPos();
            ImVec2 mousePos = ImGui::GetMousePos();

            ImVec2 MousePosInViewport;
            MousePosInViewport.x = mousePos.x - viewportScreenPos.x;
            MousePosInViewport.y = mousePos.y - viewportScreenPos.y;

            MousePosInViewport.x = glm::clamp(MousePosInViewport.x, 0.0f, ViewportSize.x - 1.0f);
            MousePosInViewport.y = glm::clamp(MousePosInViewport.y, 0.0f, ViewportSize.y - 1.0f);

            float scaleX = static_cast<float>(PickerWidth) / ViewportSize.x;
            float scaleY = static_cast<float>(PickerHeight) / ViewportSize.y;

            uint32 texX = static_cast<uint32>(MousePosInViewport.x * scaleX);
            uint32 texY = static_cast<uint32>(MousePosInViewport.y * scaleY);
            
            bool bOverImGuizmo = bImGuizmoUsedOnce ? ImGuizmo::IsOver() : false;
            bool bLeftMouseButtonClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
            if ((!bOverImGuizmo) && bLeftMouseButtonClicked)
            {
                entt::entity EntityHandle = World->GetRenderer()->GetEntityAtPixel(texX, texY);
                if (EntityHandle == entt::null)
                {
                    SetSelectedEntity({});
                }
                else
                {
                    SetSelectedEntity(Entity(EntityHandle, World));
                }
            
            }
        }
    }

    void FWorldEditorTool::DrawViewportToolbar(const FUpdateContext& UpdateContext)
    {
        Super::DrawViewportToolbar(UpdateContext);

        if (World->GetPackage() != nullptr)
        {
            ImGui::SameLine();
            constexpr float ButtonWidth = 120;

            if (!bGamePreviewRunning)
            {
                if (ImGuiX::IconButton(LE_ICON_PLAY, "Play Map", 4278255360, ImVec2(ButtonWidth, 0)))
                {
                    OnGamePreviewStartRequested.Broadcast();
                }
            }
            else
            {
                if (ImGuiX::IconButton(LE_ICON_STOP, "Stop Playing", 4278190335, ImVec2(ButtonWidth, 0)))
                {
                    OnGamePreviewStopRequested.Broadcast();
                }
            }
        }
    }

    void FWorldEditorTool::PushAddComponentModal(const Entity& Entity)
    {
        TSharedPtr<ImGuiTextFilter> Filter = MakeSharedPtr<ImGuiTextFilter>();
        ToolContext->PushModal("Add Component", ImVec2(600.0f, 350.0f), [this, Entity, Filter](const FUpdateContext& Context) -> bool
        {
            bool bComponentAdded = false;

            float const tableHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() - ImGui::GetStyle().ItemSpacing.y;
            ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(40, 40, 40, 255));

            Filter->Draw("##Search", ImGui::GetContentRegionAvail().x);
            
            if (ImGui::BeginTable("Options List", 1, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(ImGui::GetContentRegionAvail().x, tableHeight)))
            {
                ImGui::PushID((int)Entity.GetHandle());
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                for(auto &&[_, MetaType]: entt::resolve())
                {
                    using namespace entt::literals;
                    auto Any = MetaType.invoke("staticstruct"_hs, {});
                    if (void** Type = Any.try_cast<void*>())
                    {
                        CStruct* Struct = *(CStruct**)Type;
                        const char* ComponentName = Struct->GetName().c_str();
                        if (!Filter->PassFilter(ComponentName))
                        {
                            continue;
                        }
                        
                        if (ImGui::Selectable(ComponentName, false, ImGuiSelectableFlags_SpanAllColumns))
                        {
                            void* RegistryPtr = &World->GetEntityRegistry(); // EnTT will try to make a copy if not passed by *.
                            (void)MetaType.invoke("addcomponent"_hs, {}, SelectedEntity.GetHandle(), RegistryPtr);
                            bComponentAdded = true;
                        }
                    }
                }
                
                ImGui::PopID();
                ImGui::EndTable();
            }
            
            ImGui::PopStyleColor();

            if (ImGui::Button("Cancel"))
            {
                return true;
            }
            
            if (bComponentAdded)
            {
                RebuildPropertyTables();
            }
            
            return bComponentAdded;
        });
    }

    void FWorldEditorTool::PushRenameEntityModal(Entity Ent)
    {
        ToolContext->PushModal("Rename Entity", ImVec2(600.0f, 350.0f), [this, Ent](const FUpdateContext& Context) -> bool
        {
            Entity CopyEntity = Ent;
            FName& Name = CopyEntity.EmplaceOrReplace<SNameComponent>().Name;
            FString CopyName = Name.ToString();
            
            if (ImGui::InputText("##Name", const_cast<char*>(CopyName.c_str()), 256, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                Name = CopyName.c_str();
                return true;
            }
            
            if (ImGui::Button("Cancel"))
            {
                return true;
            }

            return false;
        });
    }

    void FWorldEditorTool::NotifyPlayInEditorStart()
    {
        bGamePreviewRunning = true;
    }

    void FWorldEditorTool::NotifyPlayInEditorStop()
    {
         bGamePreviewRunning = false;
    }

    void FWorldEditorTool::SetWorld(CWorld* InWorld)
    {
        if (World == InWorld)
        {
            return;
        }
        
        if (World.IsValid())
        {
            World->ShutdownWorld();
            GEngine->GetEngineSubsystem<FWorldManager>()->RemoveWorld(World);
            World.MarkGarbage();
        }
        
        World = InWorld;
        GEngine->GetEngineSubsystem<FWorldManager>()->AddWorld(World);

        World->InitializeWorld();
        EditorEntity = World->SetupEditorWorld();

        SetSelectedEntity({});
        SystemsListView.MarkTreeDirty();
    }

    void FWorldEditorTool::DrawCreateEntityMenu()
    {
        ImGui::SetNextWindowSize(ImVec2(400.0f, 500.0f), ImGuiCond_Always);
    
        if (ImGui::BeginPopup("CreateEntityMenu", ImGuiWindowFlags_NoMove))
        {
            // Header
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use larger/bold font if available
            ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), LE_ICON_PLUS " Create New Entity");
            ImGui::PopFont();
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            // Search bar
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
            ImGui::SetNextItemWidth(-1);
            
            AddEntityComponentFilter.Draw(LE_ICON_FOLDER_SEARCH " Search templates...");
            
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            
            ImGui::Spacing();
            
            // Component template list
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
            
            if (ImGui::BeginChild("TemplateList", ImVec2(0, -35.0f), true))
            {
                for(auto &&[_, MetaType]: entt::resolve())
                {
                    using namespace entt::literals;
                    
                    auto Any = MetaType.invoke("staticstruct"_hs, {});
                    if (void** Type = Any.try_cast<void*>())
                    {
                        CStruct* Struct = *(CStruct**)Type;
                        if (Struct == STransformComponent::StaticStruct() || Struct == SNameComponent::StaticStruct())
                        {
                            continue;
                        }

                        
                        ImGui::PushID(Struct);
                    
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.21f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.35f, 0.45f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.3f, 0.4f, 1.0f));
                        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 10.0f));
                    
                        const float ButtonWidth = ImGui::GetContentRegionAvail().x;
                    
                        if (ImGui::Button(Struct->GetName().c_str(), ImVec2(ButtonWidth, 0.0f)))
                        {
                            CreateEntityWithComponent(Struct);
                            ImGui::CloseCurrentPopup();
                        }
                    
                        ImGui::PopStyleVar(2);
                        ImGui::PopStyleColor(3);
                        
                        ImGui::PopID();
                    
                        ImGui::Spacing();
                    }
                }
            }
            ImGui::EndChild();
            
            ImGui::PopStyleVar(2);
            
            ImGui::Separator();
            
            ImGui::BeginGroup();
            {
                if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f)))
                {
                    ImGui::CloseCurrentPopup();
                    AddEntityComponentFilter.Clear();
                }
                
                ImGui::SameLine();
                
                // Quick empty entity button
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
                if (ImGui::Button(LE_ICON_CUBE " Empty Entity", ImVec2(-1, 0.0f)))
                {
                    CreateEntity();
                    ImGui::CloseCurrentPopup();
                    AddEntityComponentFilter.Clear();
                }
                ImGui::PopStyleColor();
                
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Create entity without any components");
                }
            }
            ImGui::EndGroup();
            
            ImGui::EndPopup();
        }
    }

    void FWorldEditorTool::DrawFilterOptions()
    {
        ImGui::Text(LE_ICON_CUBE " Filter by Component");
        ImGui::Separator();

        for(auto &&[_, MetaType]: entt::resolve())
        {
            using namespace entt::literals;
            auto Any = MetaType.invoke("staticstruct"_hs, {});
            if (void** Type = Any.try_cast<void*>())
            {
                CStruct* Struct = *(CStruct**)Type;
                const char* ComponentName = Struct->GetName().c_str();

                static bool bTest = false;
                ImGui::Checkbox(ComponentName, &bTest);
                
            }
        }
        
        ImGui::Separator();
    
        if (ImGui::Button("Clear All", ImVec2(-1, 0)))
        {
            //EntityFilterState.ClearFilters();
        }
    }

    void FWorldEditorTool::SetSelectedEntity(const Entity& NewEntity)
    {
        if (NewEntity == SelectedEntity)
        {
            return;
        }

        SelectedEntity = NewEntity;
        OutlinerListView.MarkTreeDirty();
        RebuildPropertyTables();
        World->SetSelectedEntity(SelectedEntity.GetHandle());
    }

    void FWorldEditorTool::RebuildSceneOutliner(FTreeListView* View)
    {
        TFunction<void(Entity, FEntityListViewItem*)> AddEntityRecursive;
        
        AddEntityRecursive = [&](Entity entity, FEntityListViewItem* ParentItem)
        {
            FEntityListViewItem* Item = nullptr;
            if (ParentItem)
            {
                Item = ParentItem->AddChild<FEntityListViewItem>(ParentItem, eastl::move(entity));
            }
            else
            {
                Item = OutlinerListView.AddItemToTree<FEntityListViewItem>(ParentItem, eastl::move(entity));
            }

            if (Item->GetEntity() == SelectedEntity)
            {
                OutlinerListView.SetSelection(Item, OutlinerContext);
            }

            if (SRelationshipComponent* Rel = Item->GetEntity().TryGetComponent<SRelationshipComponent>())
            {
                for (SIZE_T i = 0; i < Rel->NumChildren; ++i)
                {
                    AddEntityRecursive(Rel->Children[i], Item);
                }
            }

        };
        

        for (entt::entity EntityHandle : World->GetEntityRegistry().view<entt::entity>(entt::exclude<SHiddenComponent>))
        {
            Entity entity(EntityHandle, World);

            if (SRelationshipComponent* Rel = entity.TryGetComponent<SRelationshipComponent>())
            {
                if (Rel->Parent.IsValid())
                {
                    continue;
                }
            }

            AddEntityRecursive(eastl::move(entity), nullptr);
        }
    }


    void FWorldEditorTool::HandleEntityEditorDragDrop(FTreeListViewItem* DropItem)
    {
        const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(FEntityListViewItem::DragDropID, ImGuiDragDropFlags_AcceptBeforeDelivery);
        if (Payload && Payload->IsDelivery())
        {
            uintptr_t* RawPtr = (uintptr_t*)Payload->Data;
            auto* SourceItem = (FEntityListViewItem*)*RawPtr;
            auto* DestinationItem = (FEntityListViewItem*)DropItem;

            if (SourceItem == DestinationItem)
            {
                return;
            }

            World->ReparentEntity(SourceItem->GetEntity(), DestinationItem->GetEntity());
            
            OutlinerListView.MarkTreeDirty();
        }
    }

    void FWorldEditorTool::DrawOutliner(const FUpdateContext& UpdateContext, bool bFocused)
    {
        const ImGuiStyle& Style = ImGui::GetStyle();
        const float AvailWidth = ImGui::GetContentRegionAvail().x;
        
        {
            const int EntityCount = World->GetEntityRegistry().view<entt::entity>().size();
            ImGui::BeginGroup();
            {
                ImGui::AlignTextToFramePadding();

                ImGui::BeginHorizontal("##EntityCount");
                {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), LE_ICON_CUBE " Entities");
                
                    // Count badge - use same vertical padding as the text line
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, Style.FramePadding.y));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
            
                    char CountBuf[32];
                    snprintf(CountBuf, sizeof(CountBuf), "%d", EntityCount);
                    ImGui::Button(CountBuf);
                }
                ImGui::EndHorizontal();
                
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(3);
            }
            ImGui::EndGroup();
            
            ImGui::SameLine(AvailWidth - 80.0f);
            
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.6f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.2f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            
            if (ImGui::Button(LE_ICON_PLUS " Add", ImVec2(80.0f, 0.0f)))
            {
                ImGui::OpenPopup("CreateEntityMenu");
            }
            
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Add something new to the world.");
            }

            DrawCreateEntityMenu();

        }
        
        ImGui::Spacing();
        
        // ===== Search & Filter Section =====
        {
            const float FilterButtonWidth = 30.0f;
            const float SearchWidth = AvailWidth - FilterButtonWidth - Style.ItemSpacing.x;
            
            // Search bar with icon
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
            
            ImGui::SetNextItemWidth(SearchWidth);
            EntityFilterState.FilterName.Draw("##Search");
            
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
            
            if (EntityFilterState.FilterName.InputBuf[0] == '\0')
            {
                ImDrawList* DrawList = ImGui::GetWindowDrawList();
                ImVec2 TextPos = ImGui::GetItemRectMin();
                TextPos.x += Style.FramePadding.x + 2.0f;
                TextPos.y += Style.FramePadding.y;
                DrawList->AddText(TextPos, IM_COL32(100, 100, 110, 255), LE_ICON_FILE_SEARCH " Search entities...");
            }
            
            ImGui::SameLine();
            
            const bool bFilterActive = EntityFilterState.FilterName.IsActive();
            ImGui::PushStyleColor(ImGuiCol_Button, 
                bFilterActive ? ImVec4(0.4f, 0.45f, 0.65f, 1.0f) : ImVec4(0.2f, 0.2f, 0.22f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                bFilterActive ? ImVec4(0.5f, 0.55f, 0.75f, 1.0f) : ImVec4(0.25f, 0.25f, 0.27f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            
            if (ImGui::Button(LE_ICON_FILTER_SETTINGS "##ComponentFilter", ImVec2(FilterButtonWidth, 0.0f)))
            {
                ImGui::OpenPopup("FilterPopup");
            }
            
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(bFilterActive ? "Filters active - Click to configure" : "Configure filters");
            }
            
            if (ImGui::BeginPopup("FilterPopup"))
            {
                ImGui::SeparatorText("Component Filters");
                DrawFilterOptions();
                ImGui::EndPopup();
            }
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.1f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
            
            if (ImGui::BeginChild("EntityList", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar))
            {
                OutlinerListView.Draw(OutlinerContext);
            }
            ImGui::EndChild();
            
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
        }
        
    }

    void FWorldEditorTool::DrawSystems(const FUpdateContext& UpdateContext, bool bFocused)
    {
        const ImGuiStyle& Style = ImGui::GetStyle();
        const float AvailWidth = ImGui::GetContentRegionAvail().x;
        
        {
            uint32 SystemCount = 0;
            for (uint8 i = 0; i < (uint8)EUpdateStage::Max; ++i)
            {
                SystemCount += World->GetSystemsForUpdateStage((EUpdateStage)i).size();
            }
            
            ImGui::BeginGroup();
            {
                ImGui::AlignTextToFramePadding();

                ImGui::BeginHorizontal("##SystemCount");
                {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), LE_ICON_CUBE " Systems");
                
                    // Count badge - use same vertical padding as the text line
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, Style.FramePadding.y));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
            
                    char CountBuf[32];
                    snprintf(CountBuf, sizeof(CountBuf), "%u", SystemCount);
                    ImGui::Button(CountBuf);
                }
                ImGui::EndHorizontal();
                
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(3);
            }
            ImGui::EndGroup();
            
            ImGui::SameLine(AvailWidth - 80.0f);
            
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.6f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.2f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            
            if (ImGui::Button(LE_ICON_PLUS " Add", ImVec2(80.0f, 0.0f)))
            {
                //ImGui::OpenPopup("CreateEntityMenu");
            }
            
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Add something new to the world.");
            }

            //DrawCreateEntityMenu();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.1f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
            
            if (ImGui::BeginChild("SystemList", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar))
            {
                SystemsListView.Draw(SystemsContext);
            }
            ImGui::EndChild();
            
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
        }
        
    }

    void FWorldEditorTool::DrawObjectEditor(const FUpdateContext& UpdateContext, bool bFocused)
    {
        ImGui::BeginChild("Property Editor", ImGui::GetContentRegionAvail(), true);
        
        if (SelectedEntity.IsValid())
        {

            FName EntityName = SelectedEntity.GetComponent<SNameComponent>().Name;
            if (ImGui::CollapsingHeader(EntityName.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
            {
                ImGui::BeginDisabled();
                int ID = (int)SelectedEntity.GetHandle();
                ImGui::DragInt("ID", &ID);
                ImGui::InputText("Name", (char*)EntityName.c_str(), 256);
                ImGui::EndDisabled();

                ImGui::Separator();
        
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.35f, 0.15f, 1.0f));
                if (ImGui::Button("Add", ImVec2(ImGui::GetContentRegionAvail().x / 2, 0.0f)))
                {
                    PushAddComponentModal(SelectedEntity);
                }
                ImGui::PopStyleColor();

                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.15f, 0.15f, 1.0f));
                if (ImGui::Button("Destroy", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                {
                    EntityDestroyRequests.push(SelectedEntity);
                }
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::SeparatorText("Components");
                ImGui::Spacing();
        
                for (TUniquePtr<FPropertyTable>& Table : PropertyTables)
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 6));

                    ImVec2 cursor = ImGui::GetCursorScreenPos();
                    ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight());
                    ImU32 bgColor = IM_COL32(60, 60, 60, 255);
                    ImGui::GetWindowDrawList()->AddRectFilled(cursor, ImVec2(cursor.x + size.x, cursor.y + size.y), bgColor);

                    ImGui::PushStyleColor(ImGuiCol_Header,        0);
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 0);
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  0);

                    if (ImGui::CollapsingHeader(Table->GetType()->GetName().c_str()))
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.15f, 0.15f, 1.0f));
                        ImGui::PushID(Table.get());
                
                        bool bWasRemoved = false;
                        if (Table->GetType() != STransformComponent::StaticStruct() && Table->GetType() != SNameComponent::StaticStruct())
                        {
                            if (ImGui::Button("Remove", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                            {
                                for (const auto& [ID, Set] : World->GetEntityRegistry().storage())
                                {
                                    if (Set.contains(SelectedEntity.GetHandle()))
                                    {
                                        using namespace entt::literals;

                                        auto ReturnValue = entt::resolve(Set.type()).invoke("staticstruct"_hs, {});
                                        void** Type = ReturnValue.try_cast<void*>();

                                        if (Type != nullptr && Table->GetType() == *(CStruct**)Type)
                                        {
                                            Set.remove(SelectedEntity.GetHandle());
                                            bWasRemoved = true;
                                            break;
                                        }
                                    }
                                }

                                if (!bWasRemoved)
                                {
                                    ImGuiX::Notifications::NotifyError("Failed to remove component: %s", Table->GetType()->GetName().c_str());
                                }
                            }
                        }
                
                        ImGui::PopID();
                        ImGui::PopStyleColor();

                        if (bWasRemoved)
                        {
                            ImGui::PopStyleColor(3);
                            ImGui::PopStyleVar(2);
                            RebuildPropertyTables();
                            break;
                        }
                

                        ImGui::Indent();
                        ImGuiX::Font::PushFont(ImGuiX::Font::EFont::Tiny);
                        Table->DrawTree();
                        ImGuiX::Font::PopFont();
                        ImGui::Unindent();
                    }

                    ImGui::PopStyleColor(3);
                    ImGui::PopStyleVar(2);

                    ImGui::Spacing();
                }

            }
        }
        else if (SelectedSystem && !PropertyTables.empty())
        {
            ImGui::Indent();
            ImGuiX::Font::PushFont(ImGuiX::Font::EFont::Tiny);
            PropertyTables[0]->DrawTree();
            ImGuiX::Font::PopFont();
            ImGui::Unindent();
        }
        else
        {
            ImGui::TextUnformatted("Nothing to show...");
        }

        ImGui::EndChild();

    }

    void FWorldEditorTool::DrawPropertyEditor(const FUpdateContext& UpdateContext, bool bFocused)
    {
        
    }

    void FWorldEditorTool::RebuildPropertyTables()
    {
        using namespace entt::literals;
        
        PropertyTables.clear();

        if (SelectedEntity.IsValid())
        {
            for (const auto& [ID, Set] : World->GetEntityRegistry().storage())
            {
                if (Set.contains(SelectedEntity.GetHandle()))
                {
                    if (auto func = entt::resolve(Set.type()).func("staticstruct"_hs); func)
                    {
                        auto ReturnValue = func.invoke(Set.type());
                        void** Type = ReturnValue.try_cast<void*>();
                        
                        if (Type != nullptr)
                        {
                            void* ComponentPtr = Set.value(SelectedEntity.GetHandle());
                            TUniquePtr<FPropertyTable> NewTable = MakeUniquePtr<FPropertyTable>(ComponentPtr, *(CStruct**)Type);
                            PropertyTables.emplace_back(Memory::Move(NewTable))->RebuildTree();
                        }
                    }
                }
            }
        }
        else if (SelectedSystem)
        {
            TUniquePtr<FPropertyTable> NewTable = MakeUniquePtr<FPropertyTable>(SelectedSystem, SelectedSystem->GetClass());
            PropertyTables.emplace_back(Memory::Move(NewTable))->RebuildTree();
        }
    }

    void FWorldEditorTool::CreateEntityWithComponent(const CStruct* Component)
    {
        Entity CreatedEntity;
        for(auto &&[_, MetaType]: entt::resolve())
        {
            using namespace entt::literals;
            auto Any = MetaType.invoke("staticstruct"_hs, {});
            if (void** Type = Any.try_cast<void*>())
            {
                CStruct* Struct = *(CStruct**)Type;
                if (Struct == Component)
                {
                    CreatedEntity = World->ConstructEntity("Entity");
                    CreatedEntity.GetComponent<SNameComponent>().Name = Struct->GetName().ToString() + "_" + eastl::to_string((uint32)CreatedEntity.GetHandle());
                    
                    void* RegistryPtr = &World->GetEntityRegistry(); // EnTT will try to make a copy if not passed by *.
                    (void)MetaType.invoke("addcomponent"_hs, {}, CreatedEntity.GetHandle(), RegistryPtr);
                }
            }
        }

        if (CreatedEntity.IsValid())
        {
            SetSelectedEntity(CreatedEntity);   
            OutlinerListView.MarkTreeDirty();
        }
    }

    void FWorldEditorTool::CreateEntity()
    {
        Entity NewEntity = World->ConstructEntity("Entity");
        SetSelectedEntity(NewEntity);   
        OutlinerListView.MarkTreeDirty();
    }

    void FWorldEditorTool::CopyEntity(Entity& To, const Entity& From)
    {
        World->CopyEntity(To, From);
        OutlinerListView.MarkTreeDirty();
    }

    void FWorldEditorTool::CycleGuizmoOp()
    {
        switch (GuizmoOp)
        {
        case ImGuizmo::TRANSLATE:
            {
                GuizmoOp = ImGuizmo::ROTATE;
            }
            break;
        case ImGuizmo::ROTATE:
            {
                GuizmoOp = ImGuizmo::SCALE;
            }
            break;
        case ImGuizmo::SCALE:
            {
                GuizmoOp = ImGuizmo::TRANSLATE;
            }
            break;
        }
    }
}
