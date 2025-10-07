
#include <meshoptimizer.h>
#include <fastgltf/base64.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <glm/glm.hpp>

#include "ImportHelpers.h"
#include "Memory/Memory.h"
#include "Paths/Paths.h"
#include "Renderer/MeshData.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderResource.h"
#include "Renderer/Vertex.h"

namespace Lumina::Import::Mesh::GLTF
{
    namespace
    {
        void ExtractAsset(fastgltf::Asset* OutAsset, const FString& InPath)
        {
            std::filesystem::path FSPath = InPath.c_str();
        
            fastgltf::Parser gltf_parser;
            fastgltf::GltfDataBuffer data_buffer;

            if (!data_buffer.loadFromFile(FSPath))
            {
                LOG_ERROR("Failed to load glTF model with path: {0}. Aborting import.", FSPath.string());
                return;
            }

            fastgltf::GltfType SourceType = fastgltf::determineGltfFileType(&data_buffer);

            if (SourceType == fastgltf::GltfType::Invalid)
            {
                LOG_ERROR("Failed to determine glTF file type with path: {0}. Aborting import.", FSPath.string());
                return;
            }

            constexpr fastgltf::Options options = fastgltf::Options::DontRequireValidAssetMember |
                fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers| fastgltf::Options::GenerateMeshIndices | fastgltf::Options::DecomposeNodeMatrices;

            fastgltf::Expected<fastgltf::Asset> expected_asset(fastgltf::Error::None);

            if (SourceType == fastgltf::GltfType::glTF)
            {
                expected_asset = gltf_parser.loadGltf(&data_buffer, FSPath.parent_path(), options);
            }
            else if (SourceType == fastgltf::GltfType::GLB)
            {
                expected_asset = gltf_parser.loadGltfBinary(&data_buffer, FSPath.parent_path(), options);
            }
            else
            {
                LOG_ERROR("GLTF Source Type Invalid");
                return;
            }

            if (const auto error = expected_asset.error(); error != fastgltf::Error::None)
            {
                LOG_ERROR("Failed to load asset source with path: {0}. [{1}]: {2} Aborting import.", FSPath.string(),
                fastgltf::getErrorName(error), fastgltf::getErrorMessage(error));
            }

            *OutAsset = std::move(expected_asset.get());
        }
    }
    

    void ImportGLTF(FGLTFImportData& OutData, const FGLTFImportOptions& ImportOptions, const FString& RawFilePath)
    {
        fastgltf::Asset Asset;
        ExtractAsset(&Asset, RawFilePath);
        FString Name = Paths::FileName(RawFilePath, true);
        
        OutData.Resources.clear();
        OutData.Resources.reserve(Asset.meshes.size());

        for (fastgltf::Mesh& Mesh : Asset.meshes)
        {
            TUniquePtr<FMeshResource> NewResource = MakeUniquePtr<FMeshResource>();
            NewResource->GeometrySurfaces.reserve(Mesh.primitives.size());
            NewResource->Name = Mesh.name.empty() ? FName(Name + "_" + eastl::to_string(OutData.Resources.size())) : Mesh.name.c_str();

            SIZE_T IndexCount = 0;

            
            for (auto& Material : Asset.materials)
            {
                FGLTFMaterial NewMaterial;
                NewMaterial.Name = Material.name.c_str();
                
                OutData.Materials[OutData.Resources.size()].push_back(NewMaterial);
            }

            if (ImportOptions.bImportTextures)
            {
                for (auto& Image : Asset.images)
                {
                    auto& URI = std::get<fastgltf::sources::URI>(Image.data);
                    FGLTFImage GLTFImage;
                    GLTFImage.ByteOffset = URI.fileByteOffset;
                    GLTFImage.RelativePath = URI.uri.c_str();
                    OutData.Textures.emplace(GLTFImage);
                }
            }
            
            for (auto& Primitive : Mesh.primitives)
            {
                FGeometrySurface NewSurface;
                NewSurface.StartIndex = IndexCount;
                NewSurface.ID = Mesh.name.empty() ? FName(Name + "_" + eastl::to_string(NewResource->GetNumSurfaces())) : Mesh.name.c_str();
                
                if (Primitive.materialIndex.has_value())
                {
                    NewSurface.MaterialIndex = (int64)Primitive.materialIndex.value();
                }
                
                SIZE_T InitialVert = NewResource->Vertices.size();
            
                fastgltf::Accessor& IndexAccessor = Asset.accessors[Primitive.indicesAccessor.value()];
                NewResource->Indices.reserve(NewResource->Indices.size() + IndexAccessor.count);
            
                fastgltf::iterateAccessor<uint32>(Asset, IndexAccessor, [&](uint32 Index)
                {
                    NewResource->Indices.push_back(Index);
                    NewSurface.IndexCount++;
                    IndexCount++;
                });
            
                fastgltf::Accessor& PosAccessor = Asset.accessors[Primitive.findAttribute("POSITION")->second];
                NewResource->Vertices.resize(NewResource->Vertices.size() + PosAccessor.count);
            
                // Initialize all vertices with defaults
                for (size_t i = InitialVert; i < NewResource->Vertices.size(); ++i)
                {
                    NewResource->Vertices[i].Normal = PackNormal(glm::vec3(0.0f, 1.0f, 0.0f)); // Default up normal
                    NewResource->Vertices[i].UV = glm::u16vec2(0, 0);
                    NewResource->Vertices[i].Color = 0xFFFFFFFF; // White
                }
            
                // Load positions
                fastgltf::iterateAccessorWithIndex<glm::vec3>(Asset, PosAccessor, [&](glm::vec3 V, size_t Index)
                {
                    NewResource->Vertices[InitialVert + Index].Position = V;
                });
            
                // Load normals
                auto normals = Primitive.findAttribute("NORMAL");
                if (normals != Primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(Asset, Asset.accessors[normals->second], [&](glm::vec3 v, size_t index)
                    {
                        NewResource->Vertices[InitialVert + index].Normal = PackNormal(glm::normalize(v));
                    });
                }
            
                // Load UVs
                auto uv = Primitive.findAttribute("TEXCOORD_0");
                if (uv != Primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(Asset, Asset.accessors[uv->second], [&](glm::vec2 v, size_t index)
                    {
                        // Convert float UVs to uint16 (0.0-1.0 -> 0-65535)
                        if (ImportOptions.bFlipUVs)
                        {
                            v.y = 1.0f - v.y;
                        }
                        NewResource->Vertices[InitialVert + index].UV.x = (uint16)(glm::clamp(v.x, 0.0f, 1.0f) * 65535.0f);
                        NewResource->Vertices[InitialVert + index].UV.y = (uint16)(glm::clamp(v.y, 0.0f, 1.0f) * 65535.0f);
                    });
                }
            
                // Load vertex colors
                auto colors = Primitive.findAttribute("COLOR_0");
                if (colors != Primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(Asset, Asset.accessors[colors->second], [&](glm::vec4 v, size_t index)
                    {
                        NewResource->Vertices[InitialVert + index].Color = PackColor(v);
                    });
                }
                
                NewResource->GeometrySurfaces.push_back(NewSurface);
            }
            
            if (ImportOptions.bOptimize)
            {
                for (FGeometrySurface& Section : NewResource->GeometrySurfaces)
                {
                    meshopt_optimizeVertexCache(&NewResource->Indices[Section.StartIndex], &NewResource->Indices[Section.StartIndex], Section.IndexCount, NewResource->Vertices.size());
                    
                    // Reorder indices for overdraw, balancing overdraw and vertex cache efficiency.
                    // Allow up to 5% worse ACMR to get more reordering opportunities for overdraw.
                    constexpr float Threshold = 1.05f;
                    meshopt_optimizeOverdraw(&NewResource->Indices[Section.StartIndex], &NewResource->Indices[Section.StartIndex], Section.IndexCount, (float*)NewResource->Vertices.data(), NewResource->Vertices.size(), sizeof(FVertex), Threshold);
                }

                // Vertex fetch optimization should go last as it depends on the final index order
                meshopt_optimizeVertexFetch(NewResource->Vertices.data(), NewResource->Indices.data(), NewResource->Indices.size(), NewResource->Vertices.data(), NewResource->Vertices.size(), sizeof(FVertex));
            }

            NewResource->ShadowIndices = TVector<uint32>(NewResource->Indices.size());
            meshopt_generateShadowIndexBuffer(NewResource->ShadowIndices.data(), NewResource->Indices.data(), NewResource->Indices.size(), &NewResource->Vertices[0].Position, NewResource->Vertices.size(), sizeof(glm::vec4), sizeof(FVertex));
            meshopt_optimizeVertexCache(NewResource->ShadowIndices.data(), NewResource->ShadowIndices.data(), NewResource->ShadowIndices.size(), NewResource->Vertices.size());


            OutData.VertexFetchStatics.push_back(meshopt_analyzeVertexFetch(NewResource->Indices.data(), NewResource->Indices.size(), NewResource->Vertices.size(), sizeof(FVertex)));
            OutData.OverdrawStatics.push_back(meshopt_analyzeOverdraw(NewResource->Indices.data(), NewResource->Indices.size(), (float*)NewResource->Vertices.data(), NewResource->Vertices.size(), sizeof(FVertex)));
            OutData.Resources.push_back(eastl::move(NewResource));
        }
    }
}
