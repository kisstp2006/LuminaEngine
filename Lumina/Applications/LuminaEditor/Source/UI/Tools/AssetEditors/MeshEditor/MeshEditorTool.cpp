#include "MeshEditorTool.h"

#include "ImGuiDrawUtils.h"
#include "Core/Object/Cast.h"
#include "Core/Reflection/Type/LuminaTypes.h"
#include "glm/glm.hpp"
#include "glm/gtx/string_cast.hpp"
#include "Tools/UI/ImGui/ImGuiFonts.h"
#include "Tools/UI/ImGui/ImGuiX.h"
#include "world/entity/components/environmentcomponent.h"
#include "World/Entity/Components/LightComponent.h"
#include "World/Entity/Components/StaticMeshComponent.h"
#include "World/Entity/Components/VelocityComponent.h"
#include "World/Scene/RenderScene/RenderScene.h"
#include "World/Scene/RenderScene/SceneRenderTypes.h"


namespace Lumina
{
    static const char* MeshPropertiesName        = "MeshProperties";

    FMeshEditorTool::FMeshEditorTool(IEditorToolContext* Context, CObject* InAsset)
    : FAssetEditorTool(Context, InAsset->GetName().c_str(), InAsset)
    {
        World = NewObject<CWorld>();

        Entity DirectionalLightEntity = World->ConstructEntity("Directional Light");
        DirectionalLightEntity.Emplace<SDirectionalLightComponent>();
        DirectionalLightEntity.Emplace<SEnvironmentComponent>();

        MeshEntity = World->ConstructEntity("MeshEntity");
        
        MeshEntity.Emplace<SStaticMeshComponent>().StaticMesh = Cast<CStaticMesh>(InAsset);
        MeshEntity.GetComponent<STransformComponent>().SetLocation(glm::vec3(0.0f, 0.0f, -2.5f));
    }

void FMeshEditorTool::OnInitialize()
{
    CreateToolWindow(MeshPropertiesName, [this](const FUpdateContext& Cxt, bool bFocused)
    {
        CStaticMesh* StaticMesh = Cast<CStaticMesh>(Asset.Get());
        if (!StaticMesh)
        {
            return;
        }

        const FMeshResource& Resource = StaticMesh->GetMeshResource();
        const FAABB& BoundingBox = StaticMesh->GetAABB();
        
        // === Mesh Statistics Overview ===
        ImGuiX::Font::PushFont(ImGuiX::Font::EFont::Large);
        ImGui::SeparatorText("Mesh Statistics");
        ImGuiX::Font::PopFont();
        
        ImGui::Spacing();
        
        if (ImGui::BeginTable("##MeshStats", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            auto PropertyRow = [](const char* label, const FString& value, const ImVec4* color = nullptr)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(label);
                ImGui::TableSetColumnIndex(1);
                if (color)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, *color);
                }
                ImGui::TextUnformatted(value.c_str());
                if (color)
                {
                    ImGui::PopStyleColor();
                }
            };

            // Geometry counts
            PropertyRow("Vertices", eastl::to_string(Resource.Vertices.size()));
            PropertyRow("Triangles", eastl::to_string(Resource.Indices.size() / 3));
            PropertyRow("Indices", eastl::to_string(Resource.Indices.size()));
            PropertyRow("Shadow Indices", eastl::to_string(Resource.ShadowIndices.size()));
            PropertyRow("Surfaces", eastl::to_string(Resource.GetNumSurfaces()));
            
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Dummy(ImVec2(0, 4));
            
            // Memory usage
            const float vertexSizeKB = (Resource.Vertices.size() * sizeof(FVertex)) / 1024.0f;
            const float indexSizeKB = (Resource.Indices.size() * sizeof(uint32_t)) / 1024.0f;
            const float totalSizeKB = vertexSizeKB + indexSizeKB;
            
            PropertyRow("Vertex Buffer", eastl::to_string(static_cast<int>(vertexSizeKB)) + " KB");
            PropertyRow("Index Buffer", eastl::to_string(static_cast<int>(indexSizeKB)) + " KB");
            
            ImVec4 totalColor = totalSizeKB > 1024 ? ImVec4(1.0f, 0.7f, 0.3f, 1.0f) : ImVec4(0.7f, 1.0f, 0.7f, 1.0f);
            PropertyRow("Total Memory", eastl::to_string(static_cast<int>(totalSizeKB)) + " KB", &totalColor);
            
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Dummy(ImVec2(0, 4));
            
            // Bounding box
            PropertyRow("Bounds Min", glm::to_string(BoundingBox.Min).c_str());
            PropertyRow("Bounds Max", glm::to_string(BoundingBox.Max).c_str());
            
            glm::vec3 extents = BoundingBox.Max - BoundingBox.Min;
            PropertyRow("Bounds Extents", glm::to_string(extents).c_str());

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // === Geometry Surfaces ===
        ImGuiX::Font::PushFont(ImGuiX::Font::EFont::Large);
        ImGui::SeparatorText("Geometry Surfaces");
        ImGuiX::Font::PopFont();
        
        ImGui::Spacing();
        
        if (Resource.GeometrySurfaces.empty())
        {
            ImGui::TextDisabled("No surfaces defined");
        }
        else
        {
            for (size_t i = 0; i < Resource.GeometrySurfaces.size(); ++i)
            {
                const FGeometrySurface& Surface = Resource.GeometrySurfaces[i];
                ImGui::PushID(static_cast<int>(i));
                
                FString headerLabel = "Surface " + eastl::to_string(i) + ": " + Surface.ID;
                if (ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::Indent(16.0f);
                    
                    if (ImGui::BeginTable("##SurfaceDetails", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
                    {
                        ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                        ImGui::TableSetupColumn("##Value", ImGuiTableColumnFlags_WidthStretch);
                        
                        auto DetailRow = [](const char* label, const FString& value)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", label);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextUnformatted(value.c_str());
                        };
                        
                        DetailRow("Material Index:", eastl::to_string(Surface.MaterialIndex));
                        DetailRow("Start Index:", eastl::to_string(Surface.StartIndex));
                        DetailRow("Index Count:", eastl::to_string(Surface.IndexCount));
                        DetailRow("Triangle Count:", eastl::to_string(Surface.IndexCount / 3));
                        
                        ImGui::EndTable();
                    }
                    
                    ImGui::Unindent(16.0f);
                }
                
                ImGui::PopID();
                
                if (i < Resource.GeometrySurfaces.size() - 1)
                {
                    ImGui::Spacing();
                }
            }
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // === Asset Properties ===
        ImGuiX::Font::PushFont(ImGuiX::Font::EFont::Large);
        ImGui::SeparatorText("Asset Details");
        ImGuiX::Font::PopFont();
        
        ImGui::Spacing();
        PropertyTable.DrawTree();
    });
}

    void FMeshEditorTool::OnDeinitialize(const FUpdateContext& UpdateContext)
    {
    }

    void FMeshEditorTool::OnAssetLoadFinished()
    {
    }

    void FMeshEditorTool::DrawToolMenu(const FUpdateContext& UpdateContext)
    {
        FAssetEditorTool::DrawToolMenu(UpdateContext);

        if (ImGui::BeginMenu(LE_ICON_CAMERA_CONTROL" Camera Control"))
        {
            float Speed = EditorEntity.GetComponent<SVelocityComponent>().Speed;
            ImGui::SliderFloat("Camera Speed", &Speed, 1.0f, 200.0f);
            EditorEntity.GetComponent<SVelocityComponent>().Speed = Speed;
            ImGui::EndMenu();
        }
        
        // Gizmo Control Dropdown
        if (ImGui::BeginMenu(LE_ICON_MOVE_RESIZE " Gizmo Control"))
        {
            const char* operations[] = { "Translate", "Rotate", "Scale" };
            static int currentOp = 0;

            if (ImGui::Combo("##", &currentOp, operations, IM_ARRAYSIZE(operations)))
            {
                switch (currentOp)
                {
                case 0: GuizmoOp = ImGuizmo::TRANSLATE; break;
                case 1: GuizmoOp = ImGuizmo::ROTATE;    break;
                case 2: GuizmoOp = ImGuizmo::SCALE;     break;
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(LE_ICON_DEBUG_STEP_INTO " Render Debug"))
        {
            IRenderScene* SceneRenderer = World->GetRenderer();

            ImGui::TextColored(ImVec4(0.58f, 0.86f, 1.0f, 1.0f), "Debug Visualization");
            ImGui::Separator();

            static const char* DebugLabels[] =
            {
                "None",
                "Position",
                "Normals",
                "Albedo",
                "SSAO",
                "Material",
                "Depth",
                "Overdraw",
                "Cascade1",
                "Cascade2",
                "Cascade3",
                "Cascade4",
            };

            ERenderSceneDebugFlags DebugMode = SceneRenderer->GetDebugMode();
            int DebugModeInt = static_cast<int>(DebugMode);
            ImGui::PushItemWidth(200);
            if (ImGui::Combo("Debug Visualization", &DebugModeInt, DebugLabels, IM_ARRAYSIZE(DebugLabels)))
            {
                SceneRenderer->SetDebugMode(static_cast<ERenderSceneDebugFlags>(DebugModeInt));
            }
            ImGui::PopItemWidth();

            ImGui::EndMenu();
        }
    }

    void FMeshEditorTool::InitializeDockingLayout(ImGuiID InDockspaceID, const ImVec2& InDockspaceSize) const
    {
        ImGuiID leftDockID = 0, rightDockID = 0, bottomDockID = 0;

        ImGui::DockBuilderSplitNode(InDockspaceID, ImGuiDir_Right, 0.3f, &rightDockID, &leftDockID);

        ImGui::DockBuilderSplitNode(InDockspaceID, ImGuiDir_Down, 0.3f, &bottomDockID, &InDockspaceID);

        ImGui::DockBuilderDockWindow(GetToolWindowName(ViewportWindowName).c_str(), leftDockID);
        ImGui::DockBuilderDockWindow(GetToolWindowName(MeshPropertiesName).c_str(), rightDockID);
    }
}
