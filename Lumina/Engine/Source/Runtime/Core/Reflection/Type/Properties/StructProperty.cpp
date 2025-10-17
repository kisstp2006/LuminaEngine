#include "StructProperty.h"

namespace Lumina
{
    void FStructProperty::Serialize(FArchive& Ar, void* Value)
    {
        Struct->SerializeTaggedProperties(Ar, Value);
    }

    void FStructProperty::SerializeItem(IStructuredArchive::FSlot Slot, void* Value, void const* Defaults)
    {
    }
}
