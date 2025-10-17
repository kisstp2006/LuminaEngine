#include "EnumProperty.h"

#include "imgui.h"

namespace Lumina
{
    FEnumProperty::~FEnumProperty()
    {
        Memory::Delete(InnerProperty);
    }

    void FEnumProperty::SetEnum(CEnum* InEnum)
    {
        Enum = InEnum;
    }

    void FEnumProperty::Serialize(FArchive& Ar, void* Value)
    {
        if (Ar.IsReading())
        {
            FName EnumName;
            Ar << EnumName;
            
            int64 EnumValue = (int64)Enum->GetEnumValueByName(EnumName);
            InnerProperty->SetIntPropertyValue(Value, EnumValue);

        }
        else
        {
            const int64 IntValue = InnerProperty->GetSignedIntPropertyValue(Value);
            FName EnumName = Enum->GetNameAtValue(IntValue);
            Ar << EnumName;
        }
    }

    void FEnumProperty::SerializeItem(IStructuredArchive::FSlot Slot, void* Value, void const* Defaults)
    {
        LUMINA_NO_ENTRY()
    }
}
