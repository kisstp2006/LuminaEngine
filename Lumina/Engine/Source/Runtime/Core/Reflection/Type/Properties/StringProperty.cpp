#include "StringProperty.h"

namespace Lumina
{
    void FStringProperty::Serialize(FArchive& Ar, void* Value)
    {
        FString* StringValue = (FString*)Value;
        Ar << *StringValue;
    }

    void FStringProperty::SerializeItem(IStructuredArchive::FSlot Slot, void* Value, void const* Defaults)
    {
    }

    void FNameProperty::SerializeItem(IStructuredArchive::FSlot Slot, void* Value, void const* Defaults)
    {
    }

    void FNameProperty::Serialize(FArchive& Ar, void* Value)
    {
        FName* StringValue = (FName*)Value;
        Ar << *StringValue;
    }
}
