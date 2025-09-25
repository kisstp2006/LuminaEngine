#pragma once
#include "Platform/GenericPlatform.h"


namespace Lumina
{
    class FRHIBuffer;
    class CMaterialInterface;


    /**
     * A mesh draw command fully encapsulates all the data needed to draw a mesh draw call.
     * Mesh draw commands are cached in the scene.
     */
    struct FMeshDrawCommand
    {
        CMaterialInterface* Material    = nullptr;
        FRHIBuffer*         IndexBuffer = nullptr;
        FRHIBuffer*         VertexBuffer;
        uint32              NumDraws;
        uint32              IndirectDrawOffset;
    };

    static_assert(sizeof(FMeshDrawCommand) <= 32, "FMeshDrawCommand should be at most 1/2 the size of a typical 64 bit cache line");
    
    
}
