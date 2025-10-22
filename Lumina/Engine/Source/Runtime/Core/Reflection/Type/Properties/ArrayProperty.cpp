#include "ArrayProperty.h"

namespace Lumina
{
    void FArrayProperty::Serialize(FArchive& Ar, void* Value)
    {
        SIZE_T ElementCount = GetNum(Value);
        
        if (Ar.IsWriting())
        {
            Ar << ElementCount;
            for (SIZE_T i = 0; i < ElementCount; i++)
            {
                Inner->Serialize(Ar, GetAt(Value, i));
            }
        }
        else
        {
            Ar << ElementCount;

            for (int i = 0; i < ElementCount; ++i)
            {
                PushBack(Value, nullptr);
            }

            for (SIZE_T i = 0; i < ElementCount; ++i)
            {
                Inner->Serialize(Ar, GetAt(Value, i));
            }
        }
    }

    void FArrayProperty::SerializeItem(IStructuredArchive::FSlot Slot, void* Value, void const* Defaults)
    {
        LUMINA_NO_ENTRY()
    }
}
