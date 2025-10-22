#include "ObjectProperty.h"

#include "Core/Object/Object.h"
#include "Core/Object/ObjectHandleTyped.h"


namespace Lumina
{
    
    void FObjectProperty::Serialize(FArchive& Ar, void* Value)
    {
        FObjectHandle& Ptr = *(FObjectHandle*)Value;
        Ar << Ptr;
    }

    void FObjectProperty::SerializeItem(IStructuredArchive::FSlot Slot, void* Value, void const* Defaults)
    {
        LUMINA_NO_ENTRY()
    }
}
