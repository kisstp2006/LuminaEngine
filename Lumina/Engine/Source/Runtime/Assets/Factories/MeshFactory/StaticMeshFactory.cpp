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


namespace Lumina
{
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

        ImGui::SeparatorText("Import Options");
        
        if (ImGui::BeginTable("GLTFImportOptionsTable", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Option", ImGuiTableColumnFlags_WidthStretch, 0.4f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.6f);
    
            auto AddCheckboxRow = [&](const char* Label, bool& Option)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", Label);
                ImGui::TableSetColumnIndex(1);
                if (ImGui::Checkbox(("##" + FString(Label)).c_str(), &Option))
                {
                    bShouldReimport = true;
                }
            };
    
            AddCheckboxRow("Optimize Mesh", Options.bOptimize);
            AddCheckboxRow("Import Materials", Options.bImportMaterials);
            if (!Options.bImportMaterials)
            {
                AddCheckboxRow("Import Textures", Options.bImportTextures);
            }
            AddCheckboxRow("Import Animations", Options.bImportAnimations);
            AddCheckboxRow("Generate Tangents", Options.bGenerateTangents);
            AddCheckboxRow("Merge Meshes", Options.bMergeMeshes);
            AddCheckboxRow("Apply Transforms", Options.bApplyTransforms);
            AddCheckboxRow("Use Mesh Compression", Options.bUseCompression);
    
            // Import Scale
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Scale");
            ImGui::TableSetColumnIndex(1);
            ImGui::DragFloat("##ImportScale", &Options.Scale, 0.01f, 0.01f, 100.0f, "%.2f");
    
            ImGui::EndTable();
        }
    
        if (!ImportedData.Resources.empty())
        {
            ImGui::SeparatorText("Import Stats");

            if (ImGui::BeginTable("GLTFImportMeshStats", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
            {
                ImGui::TableSetupColumn("Mesh Name");
                ImGui::TableSetupColumn("Vertices");
                ImGui::TableSetupColumn("Indices");
                ImGui::TableSetupColumn("Surfaces");
                ImGui::TableSetupColumn("Overdraw");
                ImGui::TableSetupColumn("Vertex Fetch");
                ImGui::TableHeadersRow();

                auto DrawRow = [](const FMeshResource& Resource, const auto& Overdraw, const auto& VertexFetch)
                {
                    ImGui::TableNextRow();
                    auto setCol = [&](int idx, const char* fmt, auto value)
                    {
                        ImGui::TableSetColumnIndex(idx);
                        ImGui::Text(fmt, value);
                    };

                    setCol(0, "%s", Resource.Name.c_str());
                    setCol(1, "%zu", Resource.Vertices.size());
                    setCol(2, "%zu", Resource.Indices.size());
                    setCol(3, "%zu", Resource.GeometrySurfaces.size());
                    setCol(4, "%.2f", Overdraw.overdraw);
                    setCol(5, "%.2f", VertexFetch.overfetch);
                };

                for (size_t i = 0; i < ImportedData.Resources.size(); ++i)
                {
                    DrawRow(ImportedData.Resources[i], ImportedData.OverdrawStatics[i], ImportedData.VertexFetchStatics[i]);
                }
    
                ImGui::EndTable();
            }

            if (!ImportedData.Textures.empty())
            {
                ImGui::SeparatorText("Import Textures");

                if (ImGui::BeginTable("Import Textures", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
                {
                    ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                    ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    for (Import::Mesh::GLTF::FGLTFImage& Image : ImportedData.Textures)
                    {
                        FString ImagePath = Paths::Parent(RawPath) + "/" + Image.RelativePath;
                        
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Image(FUITextureCache::Get().GetImTexture(ImagePath), ImVec2(126.0f, 126.0f));

                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextWrapped("%s", ImagePath.c_str());
                    }
                    ImGui::EndTable();
                }

            }
        }
    
        if (ImGui::Button("Import"))
        {
            bShouldImport = true;
            bShouldClose = true;
        }
    
        ImGui::SameLine();
    
        if (ImGui::Button("Cancel"))
        {
            bShouldImport = false;
            bShouldClose = true;
        }
        
        return bShouldImport;
    }

    
    void CStaticMeshFactory::TryImport(const FString& RawPath, const FString& DestinationPath)
    {
        FString VirtualPath = Paths::ConvertToVirtualPath(DestinationPath);
        
        uint32 Counter = 0;
        for (const FMeshResource& MeshResource : ImportedData.Resources)
        {
            FString QualifiedPath = DestinationPath + eastl::to_string(Counter);
            FString FileName = Paths::FileName(QualifiedPath, true);
            
            FString FullPath = QualifiedPath;
            Paths::AddPackageExtension(FullPath);

            CStaticMesh* NewMesh = Cast<CStaticMesh>(TryCreateNew(QualifiedPath));
            NewMesh->SetFlag(OF_NeedsPostLoad);

            NewMesh->MeshResources = MeshResource;

            Task::ParallelFor((uint32)ImportedData.Textures.size(), [&](uint32 Index)
            {
                CTextureFactory* TextureFactory = CTextureFactory::StaticClass()->GetDefaultObject<CTextureFactory>();

                const Import::Mesh::GLTF::FGLTFImage& Image = ImportedData.Textures[Index];
                FString ParentPath = Paths::Parent(RawPath);
                FString TexturePath = ParentPath + "/" + Image.RelativePath;
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
