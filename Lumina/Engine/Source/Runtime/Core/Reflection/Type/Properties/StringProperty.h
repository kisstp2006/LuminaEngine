#pragma once
#include "Core/Reflection/Type/LuminaTypes.h"

namespace Lumina
{
    class FStringProperty : public FProperty
    {
    public:

        FStringProperty(const FFieldOwner& InOwner, const FPropertyParams* Params)
            :FProperty(InOwner, Params)
        {
            SetElementSize(sizeof(FString));
        }

        void Serialize(FArchive& Ar, void* Value) override;
        void SerializeItem(IStructuredArchive::FSlot Slot, void* Value, void const* Defaults) override;
    
    };


    class FNameProperty : public FProperty
    {
    public:
        
        FNameProperty(FFieldOwner InOwner, const FPropertyParams* Params)
            :FProperty(InOwner, Params)
        {
            SetElementSize(sizeof(FName));
        }

        void Serialize(FArchive& Ar, void* Value) override;
        void SerializeItem(IStructuredArchive::FSlot Slot, void* Value, void const* Defaults) override;

    };
    
}
