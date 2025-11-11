#pragma once
#include "Renderer/RHIFwd.h"

namespace Lumina
{
    class CObject;
}

namespace Lumina::ThumbnailGenerator
{
    LUMINA_API FRHIImageRef GenerateImageForObject(CObject* Object);
 
}
