#pragma once
#include <glm/glm.hpp>
#include "Containers/Array.h"
#include "Platform/GenericPlatform.h"

namespace Lumina
{
    class FRHIBuffer;
    class CMaterialInterface;
    class FDeferredRenderScene;

    /** A batch of mesh elements that contain the same material and vertex buffer. */
    struct FMeshBatch
    {
        struct FElement
        {
            uint32          FirstIndex;
            uint32          NumIndices;
        };
        
        TFixedVector<FElement, 1>   Elements;
        
        FDeferredRenderScene*               Scene;
        CMaterialInterface*         Material;
        
        FRHIBuffer*                 VertexBuffer;
        FRHIBuffer*                 IndexBuffer;

        /** Index into the scene's mesh batch array */
        SIZE_T                      IndexID;

        glm::mat4                   RenderTransform;
    };
}
