#pragma once

#include "Containers/Array.h"
#include "glm/glm.hpp"


namespace Lumina
{
    struct SStaticMeshComponent;
    class FRenderScene;
    struct FMeshBatch;
    class CMaterialInterface;
    class CStaticMesh;
    
    struct FScenePrimitive
    {
        TVector<FMeshBatch> MeshBatches;
        glm::mat4           RenderTransform;
    };
    
    struct FStaticMeshScenePrimitive : FScenePrimitive
    {
        FStaticMeshScenePrimitive(const SStaticMeshComponent& StaticMeshComponent, const glm::mat4& InRenderTransform, FRenderScene* InScene);
        
        const CStaticMesh* StaticMesh;
    };
    
}
