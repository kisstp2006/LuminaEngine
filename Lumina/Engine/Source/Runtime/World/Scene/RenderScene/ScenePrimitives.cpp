#include "ScenePrimitives.h"

#include "MeshBatch.h"
#include "assets/assettypes/material/materialinterface.h"
#include "Assets/AssetTypes/Mesh/StaticMesh/StaticMesh.h"
#include "world/entity/components/staticmeshcomponent.h"

namespace Lumina
{
    FStaticMeshScenePrimitive::FStaticMeshScenePrimitive(const SStaticMeshComponent& StaticMeshComponent, const glm::mat4& InRenderTransform, FDeferredRenderScene* InScene)
        : StaticMesh(StaticMeshComponent.StaticMesh)
    {
        RenderTransform = InRenderTransform;
        
        LUM_ASSERT(IsValid(StaticMesh))
        
        StaticMeshComponent.StaticMesh->ForEachSurface([&](const FGeometrySurface& Surface, uint32 Index)
        {
            CMaterialInterface* Material = StaticMeshComponent.GetMaterialForSlot(Index);
            bool bFound = false;

            for (size_t i = 0; i < MeshBatches.size(); ++i)
            {
                FMeshBatch& Batch = MeshBatches[i];
                LUM_ASSERT(Material->IsReadyForRender())
                
                if (Batch.Material == Material)
                {
                    bFound = true;
                    FMeshBatch::FElement Element;
                    Element.FirstIndex = Surface.StartIndex;
                    Element.NumIndices = Surface.IndexCount;
                    
                    Batch.Elements.push_back(Element);
                }
            }

            if (!bFound)
            {
                FMeshBatch Batch;
                Batch.Material = Material;
                Batch.RenderTransform = RenderTransform;
                Batch.IndexBuffer = StaticMeshComponent.StaticMesh->GetIndexBuffer();
                Batch.VertexBuffer = StaticMeshComponent.StaticMesh->GetVertexBuffer();
                Batch.Scene = InScene;

                MeshBatches.push_back(std::move(Batch));

                FMeshBatch::FElement Element;
                Element.FirstIndex = Surface.StartIndex;
                Element.NumIndices = Surface.IndexCount;

                MeshBatches.back().Elements.push_back(Element);
            }
        });  
    }
}
