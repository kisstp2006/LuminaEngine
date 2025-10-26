#pragma once

#include "Module/API.h"
#include "Platform/GenericPlatform.h"


namespace Lumina
{
    class CObject;
    class CObjectBase;
}

namespace Lumina
{
    // Object handle for weak reference semantics
    struct LUMINA_API FObjectHandle
    {
        int32 Index = -1;
        int32 Generation = 0;

        FObjectHandle() = default;
        FObjectHandle(const CObjectBase* InObject);
        FObjectHandle(int32 InIndex, int32 InGeneration)
            : Index(InIndex), Generation(InGeneration)
        {}


        CObject* Resolve() const;
        
        bool IsValid() const
        {
            return Index >= 0;
        }

        bool operator==(const FObjectHandle& Other) const
        {
            return Index == Other.Index && Generation == Other.Generation;
        }

        bool operator!=(const FObjectHandle& Other) const
        {
            return !(*this == Other);
        }
    };
}
