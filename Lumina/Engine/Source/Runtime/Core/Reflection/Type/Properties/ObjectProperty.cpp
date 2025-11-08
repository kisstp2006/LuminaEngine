#include "ObjectProperty.h"

#include "Core/Object/Object.h"
#include "Core/Object/ObjectHandleTyped.h"


namespace Lumina
{
    
    void FObjectProperty::Serialize(FArchive& Ar, void* Value)
    {
        TObjectPtr<CObject>* Ptr = static_cast<TObjectPtr<CObject>*>(Value);

        if (Ar.IsWriting())
        {
            CObject* Raw = Ptr->Get();
            Ar << Raw;
            *Ptr = Raw;
        }
        else if (Ar.IsReading())
        {
            CObject* Raw = nullptr;
            Ar << Raw;
            *Ptr = Raw;
        }
    }

    void FObjectProperty::SerializeItem(IStructuredArchive::FSlot Slot, void* Value, void const* Defaults)
    {
        LUMINA_NO_ENTRY()
    }
}
