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

            const FMeshResource& Resource = StaticMesh->GetMeshResource();
            TVector<FGeometrySurface> Surfaces = Resource.GeometrySurfaces;
            
            if (ImGui::CollapsingHeader("Mesh Resources", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Indent();

                if (ImGui::BeginTable("MeshResourceTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                {
                    ImGui::TableSetupColumn("Property");
                    ImGui::TableSetupColumn("Value");
                    ImGui::TableHeadersRow();

                    auto Row = [](const char* label, const FString& value)
                    {
                           ImGui::TableNextRow();
                           ImGui::TableSetColumnIndex(0);
                           ImGui::TextUnformatted(label);
                           ImGui::TableSetColumnIndex(1);
                           ImGui::TextUnformatted(value.c_str());
                    };

                    Row("Vertex Buffer Size (KB)", eastl::to_string(Resource.Vertices.size() * sizeof(FVertex) / 1024));
                    Row("Index Buffer Size (KB)", eastl::to_string(Resource.Indices.size() * sizeof(uint32_t) / 1024));
                    Row("Bounding Box Min", glm::to_string(StaticMesh->GetAABB().Min).c_str());
                    Row("Bounding Box Max", glm::to_string(StaticMesh->GetAABB().Max).c_str());
                    Row("Triangles", eastl::to_string(Resource.Vertices.size() / 3));
                    Row("Vertices", eastl::to_string(Resource.Vertices.size()));
                    Row("Indices", eastl::to_string(Resource.Indices.size()));
                    Row("Shadow Indices", eastl::to_string(Resource.ShadowIndices.size()));
                    Row("Surfaces", eastl::to_string(Resource.GetNumSurfaces()));

                    ImGui::EndTable();
                }

                ImGui::Unindent();
            }

            ImGuiX::Font::PushFont(ImGuiX::Font::EFont::Large);
            ImGui::SeparatorText("Geometry Surfaces");
            ImGuiX::Font::PopFont();
            
            for (size_t i = 0; i < Resource.GeometrySurfaces.size(); ++i)
            {
                const FGeometrySurface& Surface = Resource.GeometrySurfaces[i];
                ImGui::PushID(i);
                ImGui::Text("Name: %s", Surface.ID.c_str());
                ImGui::Text("Material Index: %lld", Surface.MaterialIndex);
                ImGui::Text("Index Count: %u", Surface.IndexCount);
                ImGui::Text("Start Index: %llu", Surface.StartIndex);

                ImGui::Separator();
                ImGui::PopID();
            }

            ImGui::SeparatorText("Asset Details");

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
            FRenderScene* SceneRenderer = World->GetRenderer();

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
