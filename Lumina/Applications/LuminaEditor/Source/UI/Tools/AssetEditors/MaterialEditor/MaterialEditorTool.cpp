#include "MaterialEditorTool.h"
#include "Assets/AssetTypes/Material/Material.h"
#include "Assets/AssetTypes/Textures/Texture.h"
#include "Core/Engine/Engine.h"
#include "Core/Object/Cast.h"
#include "Renderer/RHIIncl.h"
#include "Core/Object/Class.h"
#include "Core/Object/Package/Package.h"
#include "Paths/Paths.h"
#include "Platform/Filesystem/FileHelper.h"
#include "Renderer/MaterialTypes.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RHIGlobals.h"
#include "Renderer/ShaderCompiler.h"
#include "World/entity/components/lightcomponent.h"
#include "World/entity/components/staticmeshcomponent.h"
#include "World/Entity/Systems/EditorEntityMovementSystem.h"
#include "Thumbnails/ThumbnailManager.h"
#include "Tools/UI/ImGui/ImGuiX.h"
#include "UI/Tools/NodeGraph/Material/MaterialCompiler.h"
#include "UI/Tools/NodeGraph/Material/MaterialNodeGraph.h"
#include "world/entity/components/environmentcomponent.h"

namespace Lumina
{
    static const char* MaterialGraphName           = "Material Graph";
    static const char* MaterialPropertiesName      = "Material Properties";
    static const char* GLSLPreviewName             = "GLSL Preview";

    FMaterialEditorTool::FMaterialEditorTool(IEditorToolContext* Context, CObject* InAsset)
        : FAssetEditorTool(Context, InAsset->GetName().c_str(), InAsset)
        , CompilationResult()
        , NodeGraph(nullptr)
    {
        World = NewObject<CWorld>();

        Entity DirectionalLightEntity = World->ConstructEntity("Directional Light");
        DirectionalLightEntity.Emplace<SDirectionalLightComponent>();
        DirectionalLightEntity.Emplace<SEnvironmentComponent>();

        MeshEntity = World->ConstructEntity("MeshEntity");
        
        MeshEntity.GetComponent<STransformComponent>().SetLocation(glm::vec3(0.0f, 0.0f,  0.0f));
        
        SStaticMeshComponent& StaticMeshComponent = MeshEntity.Emplace<SStaticMeshComponent>();
        StaticMeshComponent.StaticMesh = CThumbnailManager::Get().SphereMesh;
        StaticMeshComponent.MaterialOverrides.resize(CThumbnailManager::Get().SphereMesh->Materials.size());
        StaticMeshComponent.MaterialOverrides[0] = CastAsserted<CMaterialInterface>(InAsset);
    }


    void FMaterialEditorTool::OnInitialize()
    {
        CreateToolWindow(MaterialGraphName, [this](const FUpdateContext& Cxt, bool bFocused)
        {
            DrawMaterialGraph(Cxt);
        });

        CreateToolWindow(MaterialPropertiesName, [this](const FUpdateContext& Cxt, bool bFocused)
        {
            DrawMaterialProperties(Cxt);
        });

        CreateToolWindow(GLSLPreviewName, [this](const FUpdateContext& Cxt, bool bFocused)
        {
            DrawGLSLPreview(Cxt);
        });

        FString GraphName = Asset->GetName().ToString() + "_MaterialGraph";
        NodeGraph = LoadObject<CMaterialNodeGraph>(Asset->GetPackage(), GraphName);
        if (NodeGraph == nullptr)
        {
            NodeGraph = NewObject<CMaterialNodeGraph>(Asset->GetPackage(), GraphName);
        }
        
        NodeGraph->SetMaterial(Cast<CMaterial>(Asset.Get()));
        NodeGraph->Initialize();
        NodeGraph->SetNodeSelectedCallback( [this] (CEdGraphNode* Node)
        {
            if (Node != SelectedNode)
            {
                SelectedNode = Node;

                if (SelectedNode == nullptr)
                {
                    GetPropertyTable()->SetObject(Asset, CMaterial::StaticClass());
                }
                else
                {
                    GetPropertyTable()->SetObject(Node, Node->GetClass());
                }
            }
        });
    }
    
    void FMaterialEditorTool::OnDeinitialize(const FUpdateContext& UpdateContext)
    {
        if (NodeGraph)
        {
            NodeGraph->Shutdown();
            NodeGraph = nullptr;
        }
    }

    void FMaterialEditorTool::OnAssetLoadFinished()
    {
    }
    
    void FMaterialEditorTool::DrawToolMenu(const FUpdateContext& UpdateContext)
    {
        if (ImGui::MenuItem(LE_ICON_RECEIPT_TEXT" Compile"))
        {
            Compile();
        }
    }

    void FMaterialEditorTool::DrawMaterialGraph(const FUpdateContext& UpdateContext)
    {
        NodeGraph->DrawGraph();
    }

    void FMaterialEditorTool::DrawMaterialProperties(const FUpdateContext& UpdateContext)
    {
        PropertyTable.DrawTree();
    }

    void FMaterialEditorTool::DrawMaterialPreview(const FUpdateContext& UpdateContext)
    {
        if (Asset == nullptr)
        {
            return;
        }
        
    }

    void FMaterialEditorTool::DrawGLSLPreview(const FUpdateContext& UpdateContext)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 12));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
    
        if (Tree.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
            ImGui::BeginChild("##empty_preview", ImVec2(0, 0), true);
    
            ImVec2 available = ImGui::GetContentRegionAvail();
            ImVec2 textSize = ImGui::CalcTextSize("Compile to see preview");
            ImGui::SetCursorPos(ImVec2(
                (available.x - textSize.x) * 0.5f,
                (available.y - textSize.y) * 0.5f
            ));
    
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.65f, 1.0f));
            ImGui::TextUnformatted("Compile to see preview");
            ImGui::PopStyleColor();
    
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.35f, 1.0f));
    
            ImGui::BeginChild("##glsl_preview", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.85f, 1.0f, 1.0f));
            ImGui::TextUnformatted("GLSL Shader Tree");
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Spacing();
    
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            
            if (ReplacementStart != std::string::npos && ReplacementEnd != std::string::npos)
            {
                FString BeforeReplacement = Tree.substr(0, ReplacementStart);
                FString ReplacedCode = Tree.substr(ReplacementStart, ReplacementEnd - ReplacementStart);
                FString AfterReplacement = Tree.substr(ReplacementEnd);
    
                if (!BeforeReplacement.empty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.92f, 1.0f));
                    ImGui::TextUnformatted(ReplacedCode.c_str());
                    ImGui::PopStyleColor();
                }
    
                if (!ReplacedCode.empty())
                {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                    ImVec2 textSize = ImGui::CalcTextSize(ReplacedCode.c_str());
                    
                    ImVec2 rectMin = cursorPos;
                    ImVec2 rectMax = ImVec2(cursorPos.x + textSize.x + 8.0f, cursorPos.y + textSize.y);
                    drawList->AddRectFilled(rectMin, rectMax, IM_COL32(40, 80, 60, 100));
    
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 1.0f, 0.7f, 1.0f));
                    ImGui::TextUnformatted(ReplacedCode.c_str());
                    ImGui::PopStyleColor();
                }
    
                if (!AfterReplacement.empty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.92f, 1.0f));
                    ImGui::TextUnformatted(AfterReplacement.c_str());
                    ImGui::PopStyleColor();
                }
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.92f, 1.0f));
                ImGui::TextUnformatted(Tree.c_str());
                ImGui::PopStyleColor();
            }
    
            ImGui::PopFont();
    
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
        }
    
        ImGui::PopStyleVar(2);
    }

    void FMaterialEditorTool::Compile()
    {
        CompilationResult = FCompilationResultInfo();
            
        FMaterialCompiler Compiler;
        NodeGraph->CompileGraph(&Compiler);
        if (Compiler.HasErrors())
        {
            for (const FMaterialCompiler::FError& Error : Compiler.GetErrors())
            {
                CompilationResult.CompilationLog += "ERROR - [" + Error.ErrorName + "]: " + Error.ErrorDescription + "\n";
            }
                
            CompilationResult.bIsError = true;
        }
        else
        {
            Tree = Compiler.BuildTree(ReplacementStart, ReplacementEnd);
            CompilationResult.CompilationLog = "Generated GLSL: \n \n \n" + Tree;
            CompilationResult.bIsError = false;
            bGLSLPreviewDirty = true;
            
            IShaderCompiler* ShaderCompiler = GRenderContext->GetShaderCompiler();
            ShaderCompiler->CompilerShaderRaw(Tree, {}, [this](const FShaderHeader& Header) mutable 
            {
                CMaterial* Material = Cast<CMaterial>(Asset.Get());

                FRHIPixelShaderRef PixelShader = GRenderContext->CreatePixelShader(Header);
                FRHIVertexShaderRef VertexShader = GRenderContext->GetShaderLibrary()->GetShader("GeometryPass.vert").As<FRHIVertexShader>();
                
                {
                    Material->PixelShaderBinaries.assign(Header.Binaries.begin(), Header.Binaries.end());
                    Material->PixelShader = PixelShader;
                }
                
                {
                    Material->VertexShaderBinaries.assign(VertexShader->GetShaderHeader().Binaries.begin(), VertexShader->GetShaderHeader().Binaries.end());
                    Material->VertexShader = VertexShader;
                }
                
                GRenderContext->OnShaderCompiled(PixelShader, false, true);
            });

            // Wait for shader to compile.
            ShaderCompiler->Flush();
            
            CMaterial* Material = Cast<CMaterial>(Asset.Get());
            Compiler.GetBoundTextures(Material->Textures);
            
            Memory::Memzero(&Material->MaterialUniforms, sizeof(FMaterialUniforms));
            Material->Parameters.clear();
            
            for (const auto& [Name, Value] : Compiler.GetScalarParameters())
            {
                FMaterialParameter NewParam;
                NewParam.ParameterName = Name;
                NewParam.Type = EMaterialParameterType::Scalar;
                NewParam.Index = (uint16)Value.Index;
                Material->Parameters.push_back(NewParam);
                Material->SetScalarValue(Name, Value.Value);
            }

            for (const auto& [Name, Value] : Compiler.GetVectorParameters())
            {
                FMaterialParameter NewParam;
                NewParam.ParameterName = Name;
                NewParam.Type = EMaterialParameterType::Vector;
                NewParam.Index = (uint16)Value.Index;
                Material->Parameters.push_back(NewParam);
                Material->SetVectorValue(Name, Value.Value);
            }

            Material->PostLoad();
            
            OnSave();
        }
    }


    void FMaterialEditorTool::InitializeDockingLayout(ImGuiID InDockspaceID, const ImVec2& InDockspaceSize) const
    {
        ImGuiID leftDockID = 0, rightDockID = 0;
        ImGuiID rightBottomDockID = 0;

        // 1. Split horizontally: Left (Material Graph) and Right (Material Preview + bottom)
        ImGui::DockBuilderSplitNode(InDockspaceID, ImGuiDir_Right, 0.3f, &rightDockID, &leftDockID);

        // 2. Split right dock vertically: Top (Material Preview), Bottom (GLSL Preview)
        ImGui::DockBuilderSplitNode(rightDockID, ImGuiDir_Down, 0.3f, &rightBottomDockID, &rightDockID);
        

        // Dock windows
        ImGui::DockBuilderDockWindow(GetToolWindowName(MaterialGraphName).c_str(), leftDockID);
        ImGui::DockBuilderDockWindow(GetToolWindowName(ViewportWindowName).c_str(), rightDockID);
        ImGui::DockBuilderDockWindow(GetToolWindowName(GLSLPreviewName).c_str(), rightBottomDockID);
        ImGui::DockBuilderDockWindow(GetToolWindowName(MaterialPropertiesName).c_str(), rightBottomDockID);
    }
}
