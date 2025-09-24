#pragma once

#include "glm/glm.hpp"


namespace Lumina
{
    class CMaterialInterface;
    class CStaticMesh;


    struct FScenePrimitive
    {

        
        
        /** The primitives static meshes */
        TVector<FMeshBatch> StaticMeshes;
    };
    
    struct FStaticMeshPrimitive : FScenePrimitive
    {
        CStaticMesh* StaticMesh;
        CMaterialInterface* Material;
        glm::mat4 RenderTransform;
    };
    
}
