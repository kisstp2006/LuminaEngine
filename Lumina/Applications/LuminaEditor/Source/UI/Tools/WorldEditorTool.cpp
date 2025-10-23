#include "WorldEditorTool.h"

#include "glm/gtc/type_ptr.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "EditorToolContext.h"
#include "Assets/AssetRegistry/AssetRegistry.h"
#include "Core/Object/ObjectIterator.h"
#include "Core/Object/Package/Package.h"
#include "EASTL/sort.h"
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
            FRHICommandListRef CommandList = GRenderContext->GetImmediateCommandList();

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
                "Position",
                "Normal",
                "Albedo",
                "SSAO",
                "Ambient Occlusion",
                "Roughness",
                "Metallic",
				"Specular",
                "Depth",
                "ShadowAtlas",
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
        ToolContext->PushModal("Add Component", ImVec2(650.0f, 500.0f), [this, Entity, Filter](const FUpdateContext& Context) -> bool
        {
            bool bComponentAdded = false;
    
            // Modal header styling
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
            ImGui::TextUnformatted("Select a component to add to the entity");
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
    
            // Search bar with icon and modern styling
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.16f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.19f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.2f, 0.2f, 0.21f, 1.0f));
            
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            Filter->Draw(LE_ICON_BRIEFCASE_SEARCH " Search Components...", ImGui::GetContentRegionAvail().x);
            
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
            
            ImGui::Spacing();
    
            // Component list area
            float const tableHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2;
            
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(12, 8));
            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImVec4(0.12f, 0.12f, 0.13f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, ImVec4(0.2f, 0.2f, 0.22f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.14f, 0.14f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.16f, 0.16f, 0.17f, 1.0f));
            
            if (ImGui::BeginTable("##ComponentsList", 2, 
                ImGuiTableFlags_NoSavedSettings | 
                ImGuiTableFlags_Borders | 
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY, 
                ImVec2(0, tableHeight)))
            {
                ImGui::TableSetupColumn("Component", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableHeadersRow();
    
                ImGui::PushID((int)Entity.GetHandle());
    
                // Collect and categorize components
                struct ComponentInfo
                {
                    const char* Name;
                    const char* Category;
                    entt::meta_type MetaType;
                };
                
                TVector<ComponentInfo> Components;
                
                for(auto&& [_, MetaType]: entt::resolve())
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
                        
                        // Categorize component (you can enhance this logic)
                        const char* Category = "General";
                        Components.push_back({ComponentName, Category, MetaType});
                    }
                }
                
                // Sort by category, then name
                eastl::sort(Components.begin(), Components.end(), [](const ComponentInfo& a, const ComponentInfo& b)
                {
                    int categoryCompare = strcmp(a.Category, b.Category);
                    if (categoryCompare != 0)
                    {
                        return categoryCompare < 0;
                    }
                    return strcmp(a.Name, b.Name) < 0;
                });
                
                // Draw components with category colors
                for (const ComponentInfo& CompInfo : Components)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    
                    // Color code by category
                    ImVec4 IconColor = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
                    const char* Icon = LE_ICON_CUBE;
                    
                    ImGui::PushStyleColor(ImGuiCol_Text, IconColor);
                    ImGui::TextUnformatted(Icon);
                    ImGui::PopStyleColor();
                    
                    ImGui::SameLine();
                    
                    // Component name as selectable
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.5f, 0.8f, 0.4f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.6f, 0.9f, 0.5f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.35f, 0.65f, 0.95f, 0.6f));
                    
                    if (ImGui::Selectable(CompInfo.Name, false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        using namespace entt::literals;
                        
                        void* RegistryPtr = &World->GetEntityRegistry();
                        (void)CompInfo.MetaType.invoke("addcomponent"_hs, {}, SelectedEntity.GetHandle(), RegistryPtr);
                        bComponentAdded = true;
                    }
                    
                    ImGui::PopStyleColor(3);
                    
                    // Category column
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                    ImGui::TextUnformatted(CompInfo.Category);
                    ImGui::PopStyleColor();
                }
                
                ImGui::PopID();
                ImGui::EndTable();
            }
            
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();
    
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
    
            // Bottom buttons
            float buttonWidth = 120.0f;
            float availWidth = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX((availWidth - buttonWidth) * 0.5f);
            
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 8));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.22f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.27f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.32f, 1.0f));
            
            bool shouldClose = false;
            if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
            {
                shouldClose = true;
            }
            
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
            
            if (bComponentAdded)
            {
                RebuildPropertyTables();
            }
            
            return bComponentAdded || shouldClose;
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
        World->InitializeWorld();
        GEngine->GetEngineSubsystem<FWorldManager>()->AddWorld(World);
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

        for(auto&& [_, MetaType]: entt::resolve())
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
            const SIZE_T EntityCount = World->GetEntityRegistry().view<entt::entity>().size();
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
                    snprintf(CountBuf, sizeof(CountBuf), "%llu", EntityCount);
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
        
        {
            constexpr float FilterButtonWidth = 30.0f;
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

    void FWorldEditorTool::DrawEntityProperties()
    {
        FName EntityName = SelectedEntity.GetComponent<SNameComponent>().Name;
        
        // ===== Entity Header =====
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.2f, 0.2f, 0.24f, 1.0f));
            
            if (ImGui::CollapsingHeader(EntityName.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
            {
                ImGui::Spacing();
                ImGui::Indent(8.0f);
                
                // Entity ID (read-only)
                ImGui::BeginDisabled();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
                int ID = (int)SelectedEntity.GetHandle();
                ImGui::SetNextItemWidth(-1);
                ImGui::DragInt("##ID", &ID);
                ImGui::PopStyleColor();
                ImGui::EndDisabled();
                
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Entity ID: %d", ID);
                }
                
                ImGui::Spacing();
                
                // Entity Name (read-only)
                ImGui::BeginDisabled();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##Name", (char*)EntityName.c_str(), 256);
                ImGui::PopStyleColor();
                ImGui::EndDisabled();
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                // Action Buttons
                DrawEntityActionButtons();
                
                ImGui::Spacing();
                ImGui::Unindent(8.0f);
            }
            
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
        }
        
        ImGui::Spacing();
        
        // ===== Components Section =====
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(LE_ICON_PUZZLE " Components");
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            DrawComponentList();
        }
    }

    void FWorldEditorTool::DrawEntityActionButtons()
    {
        const float ButtonHeight = 32.0f;
        const float AvailWidth = ImGui::GetContentRegionAvail().x;
        const float ButtonWidth = (AvailWidth - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    
        // Add Component Button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.65f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.5f, 0.25f, 1.0f));
    
        if (ImGui::Button(LE_ICON_PLUS " Add Component", ImVec2(ButtonWidth, ButtonHeight)))
        {
            PushAddComponentModal(SelectedEntity);
        }
    
        ImGui::PopStyleColor(3);
    
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Add a new component to this entity");
        }
    
        ImGui::SameLine();
    
        // Destroy Entity Button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.25f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.18f, 0.18f, 1.0f));
    
        if (ImGui::Button(LE_ICON_TRASH_CAN" Destroy", ImVec2(ButtonWidth, ButtonHeight)))
        {
            EntityDestroyRequests.push(SelectedEntity);
        }
    
        ImGui::PopStyleColor(3);
    
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Permanently delete this entity");
        }
    
        ImGui::PopStyleVar();
    }

    void FWorldEditorTool::DrawComponentList()
    {
        for (TUniquePtr<FPropertyTable>& Table : PropertyTables)
        {
            DrawComponentHeader(Table);
        
            ImGui::Spacing();
        }
    }

    void FWorldEditorTool::DrawComponentHeader(TUniquePtr<FPropertyTable>& Table)
    {
        const char* ComponentName = Table->GetType()->GetName().c_str();
        const bool bIsRequired = (Table->GetType() == STransformComponent::StaticStruct() || Table->GetType() == SNameComponent::StaticStruct());
        
        ImGui::PushID(Table.get());
        
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 10.0f));
        
        ImVec2 CursorPos = ImGui::GetCursorScreenPos();
        ImVec2 HeaderSize = ImVec2(ImGui::GetContentRegionAvail().x, 44.0f);
        
        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        const ImU32 HeaderBgColor = IM_COL32(25, 25, 30, 255);
        const ImU32 HeaderBorderColor = IM_COL32(45, 45, 52, 255);
        
        DrawList->AddRectFilled(CursorPos, 
            ImVec2(CursorPos.x + HeaderSize.x, CursorPos.y + HeaderSize.y), 
            HeaderBgColor, 6.0f);
        DrawList->AddRect(CursorPos, 
            ImVec2(CursorPos.x + HeaderSize.x, CursorPos.y + HeaderSize.y), 
            HeaderBorderColor, 6.0f, 0, 1.0f);
        
        // Make header clickable
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.24f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.25f, 0.3f, 0.7f));
        
        bool bIsOpen = ImGui::CollapsingHeader(("##" + FString(ComponentName)).c_str());
        
        ImGui::PopStyleColor(3);
        
        // Draw custom header content
        ImVec2 IconPos = CursorPos;
        IconPos.x += 12.0f;
        IconPos.y += (HeaderSize.y - ImGui::GetTextLineHeight()) * 0.5f;
        
        const char* Icon = LE_ICON_PUZZLE;
        if (Table->GetType() == STransformComponent::StaticStruct())
        {
            Icon = LE_ICON_MOVE_RESIZE;
        }

        DrawList->AddText(IconPos, IM_COL32(150, 170, 200, 255), Icon);
        
        // Component name
        ImVec2 NamePos = IconPos;
        NamePos.x += 30.0f;
        DrawList->AddText(NamePos, IM_COL32(220, 220, 230, 255), ComponentName);
        
        // Required badge
        if (bIsRequired)
        {
            ImVec2 BadgePos = NamePos;
            BadgePos.x += ImGui::CalcTextSize(ComponentName).x + 8.0f;
            BadgePos.y -= 2.0f;
            
            const char* BadgeText = "Required";
            ImVec2 BadgeSize = ImGui::CalcTextSize(BadgeText);
            BadgeSize.x += 12.0f;
            BadgeSize.y += 4.0f;
            
            DrawList->AddRectFilled(BadgePos, 
                ImVec2(BadgePos.x + BadgeSize.x, BadgePos.y + BadgeSize.y),
                IM_COL32(60, 80, 120, 180), 3.0f);
            
            ImVec2 BadgeTextPos = BadgePos;
            BadgeTextPos.x += 6.0f;
            BadgeTextPos.y += 2.0f;
            DrawList->AddText(BadgeTextPos, IM_COL32(180, 200, 240, 255), BadgeText);
        }
        
        // Reset cursor for content
        ImGui::SetCursorScreenPos(ImVec2(CursorPos.x, CursorPos.y + HeaderSize.y));
        
        // Draw component properties if expanded
        if (bIsOpen)
        {
            ImGui::Spacing();
            
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.12f, 1.0f));
            
            ImGui::BeginChild(("ComponentContent##" + FString(ComponentName)).c_str(), ImVec2(0, 300), true);

            // Remove button (if not required)
            if (!bIsRequired)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.25f, 0.25f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.18f, 0.18f, 1.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            
                if (ImGui::Button(LE_ICON_TRASH_CAN, ImVec2(ImGui::GetContentRegionAvail().x, 30.0f)))
                {
                    RemoveComponent(Table);
                }
            
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);
            
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Remove this component");
                }
            }
            
            ImGui::Indent(12.0f);
            ImGuiX::Font::PushFont(ImGuiX::Font::EFont::Tiny);
            Table->DrawTree();
            ImGuiX::Font::PopFont();
            ImGui::Unindent(12.0f);
            
            ImGui::EndChild();
            
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
        
        ImGui::PopStyleVar(2);
        ImGui::PopID();
    }

    void FWorldEditorTool::RemoveComponent(TUniquePtr<FPropertyTable>& Table)
    {
        bool bWasRemoved = false;
    
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

        if (bWasRemoved)
        {
            RebuildPropertyTables();
        }
        else
        {
            ImGuiX::Notifications::NotifyError("Failed to remove component: %s", 
                                              Table->GetType()->GetName().c_str());
        }
    }

    void FWorldEditorTool::DrawSystemProperties()
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        ImGui::TextUnformatted(LE_ICON_COG " System Properties");
        ImGui::PopStyleColor();
    
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.12f, 1.0f));
    
        ImGui::BeginChild("SystemContent", ImVec2(0, 0), true);
        ImGui::Indent(12.0f);
        ImGuiX::Font::PushFont(ImGuiX::Font::EFont::Tiny);
        PropertyTables[0]->DrawTree();
        ImGuiX::Font::PopFont();
        ImGui::Unindent(12.0f);
        ImGui::EndChild();
    
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    void FWorldEditorTool::DrawEmptyState()
    {
        ImVec2 WindowSize = ImGui::GetWindowSize();
        ImVec2 CenterPos = ImVec2(WindowSize.x * 0.5f, WindowSize.y * 0.5f);
    
        ImGui::SetCursorPos(ImVec2(CenterPos.x - 100.0f, CenterPos.y - 40.0f));
    
        ImGui::BeginGroup();
        {
            // Empty state icon
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.45f, 1.0f));
        
            // Center the icon
            const char* EmptyIcon = LE_ICON_INBOX;
            ImVec2 IconSize = ImGui::CalcTextSize(EmptyIcon);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (200.0f - IconSize.x) * 0.5f);
        
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Large font if available
            ImGui::TextUnformatted(EmptyIcon);
            ImGui::PopFont();
        
            ImGui::Spacing();
        
            // Empty state text
            const char* EmptyText = "Nothing selected";
            ImVec2 TextSize = ImGui::CalcTextSize(EmptyText);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (200.0f - TextSize.x) * 0.5f);
            ImGui::TextUnformatted(EmptyText);
        
            ImGui::Spacing();
        
            // Hint text
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.3f, 0.35f, 1.0f));
            const char* HintText = "Select an entity to view properties";
            ImVec2 HintSize = ImGui::CalcTextSize(HintText);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (200.0f - HintSize.x) * 0.5f);
            ImGui::TextUnformatted(HintText);
            ImGui::PopStyleColor();
        
            ImGui::PopStyleColor();
        }
        ImGui::EndGroup();
    }

    void FWorldEditorTool::DrawObjectEditor(const FUpdateContext& UpdateContext, bool bFocused)
    {
        const ImGuiStyle& Style = ImGui::GetStyle();
    
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.1f, 1.0f));
    
        ImGui::BeginChild("Property Editor", ImGui::GetContentRegionAvail(), true);
    
        if (SelectedEntity.IsValid())
        {
            DrawEntityProperties();
        }
        else if (SelectedSystem && !PropertyTables.empty())
        {
            DrawSystemProperties();
        }
        else
        {
            DrawEmptyState();
        }
    
        ImGui::EndChild();
    
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
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
