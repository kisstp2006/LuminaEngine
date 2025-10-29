#pragma once
#include "Containers/Array.h"
#include "Core/Serialization/Archiver.h"
#include "Platform/GenericPlatform.h"
#include "Renderer/RHIFwd.h"

namespace Lumina
{
    struct FPackageThumbnail
    {
        uint32 ImageWidth = 0;
        uint32 ImageHeight = 0;
        TVector<uint8> ImageData;

        FRHIImageRef LoadedImage;

        bool bDirty = true;
        
        void Serialize(FArchive& Ar)
        {
            Ar << ImageWidth;
            Ar << ImageHeight;
            Ar << ImageData;
        }
    };
}
