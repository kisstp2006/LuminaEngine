#include "StaticMeshFactory.h"

#include "ImCurveEdit.h"
#include "Assets/AssetRegistry/AssetRegistry.h"
#include "Assets/AssetTypes/Material/Material.h"
#include "Assets/AssetTypes/Mesh/StaticMesh/StaticMesh.h"
#include "Assets/Factories/TextureFactory/TextureFactory.h"
#include "Core/Object/Cast.h"
#include "Core/Object/Package/Package.h"
#include "Paths/Paths.h"
#include "Renderer/RHIIncl.h"
#include "TaskSystem/TaskSystem.h"
#include "Tools/Import/ImportHelpers.h"
#include "Tools/UI/UITextureCache.h"
#include "Tools/UI/ImGui/ImGuiDesignIcons.h"


namespace Lumina
{
    FString FormatNumber(size_t number)
    {
        FString result;
        result.sprintf("%zu", number);
        
        int insertPosition = result.length() - 3;
        while (insertPosition > 0) 
        {
            result.insert(insertPosition, ",");
            insertPosition -= 3;
        }
        return result;
    }
    
    
    CObject* CStaticMeshFactory::CreateNew(const FName& Name, CPackage* Package)
    {
        return NewObject<CStaticMesh>(Package, Name);
    }

    bool CStaticMeshFactory::DrawImportDialogue(const FString& RawPath, const FString& DestinationPath, bool& bShouldClose)
    {
        bool bShouldImport = false;
        bShouldClose = false;
        
        if (bShouldReimport)
        {
            FString VirtualPath = Paths::ConvertToVirtualPath(DestinationPath);
            ImportedData = {};
            Import::Mesh::GLTF::ImportGLTF(ImportedData, Options, RawPath);
            bShouldReimport = false;
        }
    
        // Add some padding and use a child window for better layout control
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 8));
    
        // Header with file info
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
        ImGui::TextWrapped("Importing: %s", Paths::FileName(RawPath).c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
    
        // Import Options Section
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.4f, 0.6f, 0.8f, 0.8f));
        ImGui::SeparatorText("Import Options");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        
        // Create a more spacious options layout
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(12, 8));
        if (ImGui::BeginTable("GLTFImportOptionsTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX))
        {
            ImGui::TableSetupColumn("Option", ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    
            auto AddCheckboxRow = [&](const char* Icon, const char* Label, const char* Description, bool& Option)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                
                // Add icon and label
                ImGui::Text("%s %s", Icon, Label);
                if (Description && ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", Description);
                }
                
                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
                if (ImGui::Checkbox(("##" + FString(Label)).c_str(), &Option))
                {
                    bShouldReimport = true;
                }
                ImGui::PopStyleVar();
            };
    
            AddCheckboxRow(LE_ICON_ACCOUNT_BOX, "Optimize Mesh", "Optimize vertex cache and reduce overdraw", Options.bOptimize);
            AddCheckboxRow(LE_ICON_ACCOUNT_BOX, "Import Materials", "Import material definitions from GLTF", Options.bImportMaterials);
            
            if (!Options.bImportMaterials)
            {
                // Indent texture option when materials are disabled
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Indent(20.0f);
                ImGui::Text("Import Textures");
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Import textures without materials");
                }
                ImGui::Unindent(20.0f);
                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
                if (ImGui::Checkbox("##ImportTextures", &Options.bImportTextures))
                {
                    bShouldReimport = true;
                }
                ImGui::PopStyleVar();
            }
            
            AddCheckboxRow(LE_ICON_ACCOUNT_BOX, "Import Animations", "Import skeletal and morph target animations", Options.bImportAnimations);
            AddCheckboxRow(LE_ICON_ACCOUNT_BOX, "Generate Tangents", "Calculate tangent vectors for normal mapping", Options.bGenerateTangents);
            AddCheckboxRow(LE_ICON_ACCOUNT_BOX, "Merge Meshes", "Combine compatible meshes into single objects", Options.bMergeMeshes);
            AddCheckboxRow(LE_ICON_ACCOUNT_BOX, "Apply Transforms", "Bake node transforms into vertex positions", Options.bApplyTransforms);
            AddCheckboxRow(LE_ICON_ACCOUNT_BOX, "Use Mesh Compression", "Compress vertex data to reduce memory usage", Options.bUseCompression);
            AddCheckboxRow(LE_ICON_ACCOUNT_BOX, "Flip UVs", "Flip the UVs vertically on load", Options.bFlipUVs);

            // Import Scale with better styling
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(LE_ICON_ACCOUNT_BOX " Scale");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Uniform scale factor applied to all imported geometry");
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::DragFloat("##ImportScale", &Options.Scale, 0.01f, 0.01f, 100.0f, "%.2f"))
            {
                bShouldReimport = true;
            }
            ImGui::PopStyleVar();
            ImGui::PopItemWidth();
    
            ImGui::EndTable();
        }
        ImGui::PopStyleVar(); // CellPadding
    
        // Import Statistics Section
        if (!ImportedData.Resources.empty())
        {
            ImGui::Spacing();
            ImGui::Spacing();
            
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.6f, 0.8f, 0.4f, 0.8f));
            ImGui::SeparatorText(LE_ICON_ACCOUNT_BOX "Import Statistics");
            ImGui::PopStyleColor();
            ImGui::Spacing();
    
            // Mesh Statistics with alternating row colors
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(10, 6));
            if (ImGui::BeginTable("GLTFImportMeshStats", 6, 
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | 
                ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 150)))
            {
                // Styled headers
                ImGui::TableSetupColumn("Mesh Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Vertices", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableSetupColumn("Indices", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableSetupColumn("Surfaces", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableSetupColumn("Overdraw", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableSetupColumn("V-Fetch", ImGuiTableColumnFlags_WidthFixed, 80);
                
                ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImVec4(0.3f, 0.4f, 0.5f, 1.0f));
                ImGui::TableHeadersRow();
                ImGui::PopStyleColor();
    
                auto DrawRow = [](const FMeshResource& Resource, const auto& Overdraw, const auto& VertexFetch)
                {
                    ImGui::TableNextRow();
                    
                    auto SetColoredColumn = [&](int idx, const char* fmt, auto value, ImVec4 color = ImVec4(1,1,1,1))
                    {
                        ImGui::TableSetColumnIndex(idx);
                        if (color.x != 1 || color.y != 1 || color.z != 1)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, color);
                            ImGui::Text(fmt, value);
                            ImGui::PopStyleColor();
                        }
                        else
                        {
                            ImGui::Text(fmt, value);
                        }
                    };
    
                    SetColoredColumn(0, "%s", Resource.Name.c_str());
                    
                    // Color-code vertex/index counts based on complexity
                    ImVec4 vertexColor = Resource.Vertices.size() > 10000 ? ImVec4(1,0.7f,0.3f,1) : ImVec4(0.7f,1,0.7f,1);
                    SetColoredColumn(1, "%s", FormatNumber(Resource.Vertices.size()).c_str(), vertexColor);
                    SetColoredColumn(2, "%s", FormatNumber(Resource.Indices.size()).c_str());
                    SetColoredColumn(3, "%zu", Resource.GeometrySurfaces.size());
                    
                    // Color-code performance metrics
                    ImVec4 overdrawColor = Overdraw.overdraw > 2.0f ? ImVec4(1,0.5f,0.5f,1) : ImVec4(0.8f,0.8f,0.8f,1);
                    SetColoredColumn(4, "%.2f", Overdraw.overdraw, overdrawColor);
                    
                    ImVec4 fetchColor = VertexFetch.overfetch > 2.0f ? ImVec4(1,0.5f,0.5f,1) : ImVec4(0.8f,0.8f,0.8f,1);
                    SetColoredColumn(5, "%.2f", VertexFetch.overfetch, fetchColor);
                };
    
                for (size_t i = 0; i < ImportedData.Resources.size(); ++i)
                {
                    DrawRow(*ImportedData.Resources[i], ImportedData.OverdrawStatics[i], ImportedData.VertexFetchStatics[i]);
                }
    
                ImGui::EndTable();
            }
            ImGui::PopStyleVar(); // CellPadding
    
            // Texture Preview Section
            if (!ImportedData.Textures.empty())
            {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.8f, 0.6f, 0.8f, 0.8f));
                ImGui::SeparatorText(LE_ICON_ACCOUNT_BOX "Texture Preview");
                ImGui::PopStyleColor();
                ImGui::Spacing();
    
                if (ImGui::BeginChild("ImportTexturesChild", ImVec2(0, 200), true))
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8, 8));
                    if (ImGui::BeginTable("ImportTextures", 2, 
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
                    {
                        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
                        
                        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImVec4(0.4f, 0.3f, 0.4f, 1.0f));
                        ImGui::TableHeadersRow();
                        ImGui::PopStyleColor();
    
                        for (const Import::Mesh::GLTF::FGLTFImage& Image : ImportedData.Textures)
                        {
                            FString ImagePath = Paths::Parent(RawPath) + "/" + Image.RelativePath;
                        
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            
                            // Add a subtle border around the texture preview
                            ImVec2 imageSize(128.0f, 128.0f);
                            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                            ImGui::GetWindowDrawList()->AddRect(
                                cursorPos, 
                                ImVec2(cursorPos.x + imageSize.x + 4, cursorPos.y + imageSize.y + 4),
                                IM_COL32(100, 100, 100, 255), 2.0f);
                            
                            ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + 2, cursorPos.y + 2));

                            ImGui::Image(FUITextureCache::Get().GetImTexture(ImagePath), imageSize);

                            ImGui::TableSetColumnIndex(1);
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                            ImGui::TextWrapped("%s", Paths::FileName(ImagePath).c_str());
                            ImGui::PopStyleColor();
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                            ImGui::TextWrapped("%s", ImagePath.c_str());
                            ImGui::PopStyleColor();
                        }
                        ImGui::EndTable();
                    }
                    ImGui::PopStyleVar(); // CellPadding
                    ImGui::EndChild();
                }
            }
        }
    
        // Action Buttons with better styling
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    
        // Center the buttons
        float buttonWidth = 100.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float totalWidth = (buttonWidth * 2) + spacing;
        float offsetX = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;
        if (offsetX > 0)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        }

        // Style the import button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 8));
        
        if (ImGui::Button("Import", ImVec2(buttonWidth, 0)))
        {
            bShouldImport = true;
            bShouldClose = true;
        }
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
    
        // Style the cancel button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.4f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 8));
        
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
        {
            ImportedData = {};
            Options = {};
            bShouldImport = false;
            bShouldClose = true;
        }
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        
        ImGui::PopStyleVar(2);
        
        
        return bShouldImport;
    }
    
    void CStaticMeshFactory::TryImport(const FString& RawPath, const FString& DestinationPath)
    {
        FString VirtualPath = Paths::ConvertToVirtualPath(DestinationPath);
        
        uint32 Counter = 0;
        for (TUniquePtr<FMeshResource>& MeshResource : ImportedData.Resources)
        {
            FString QualifiedPath = DestinationPath + eastl::to_string(Counter);
            FString FileName = Paths::FileName(QualifiedPath, true);
            
            FString FullPath = QualifiedPath;
            Paths::AddPackageExtension(FullPath);

            CStaticMesh* NewMesh = Cast<CStaticMesh>(TryCreateNew(QualifiedPath));
            NewMesh->SetFlag(OF_NeedsPostLoad);

            NewMesh->MeshResources = eastl::move(MeshResource);

            Task::ParallelFor(ImportedData.Textures, ImportedData.Textures.size() / 4, [&](const Import::Mesh::GLTF::FGLTFImage& Texture)
            {
                CTextureFactory* TextureFactory = CTextureFactory::StaticClass()->GetDefaultObject<CTextureFactory>();

                FString ParentPath = Paths::Parent(RawPath);
                FString TexturePath = ParentPath + "/" + Texture.RelativePath;
                FString TextureFileName = Paths::RemoveExtension(Paths::FileName(TexturePath));
                                         
                FString DestinationParent = Paths::Parent(FullPath);
                FString TextureDestination = DestinationParent + "/" + TextureFileName;
                                             
                TextureFactory->TryImport(TexturePath, TextureDestination);
            });

            for (SIZE_T i = 0; i < ImportedData.Materials[Counter].size(); ++i)
            {
                const Import::Mesh::GLTF::FGLTFMaterial& Material = ImportedData.Materials[Counter][i];
                FName MaterialName = (i == 0) ? FString(FileName + "_Material").c_str() : FString(FileName + "_Material" + eastl::to_string(i)).c_str();
                //CMaterial* NewMaterial = NewObject<CMaterial>(NewPackage, MaterialName.c_str());
                NewMesh->Materials.push_back(nullptr);
            }
            
            Counter++;
        }
        

        Options = {};
        ImportedData = {};
        bShouldReimport = true;
    }
}
