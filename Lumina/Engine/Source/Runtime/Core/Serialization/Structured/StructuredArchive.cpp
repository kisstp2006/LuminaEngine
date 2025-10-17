#include "StructuredArchive.h"


namespace Lumina
{
    FArchiveRecord FArchiveSlot::EnterRecord()
    {
        return {};
    }

    void FArchiveSlot::Serialize(uint8& Value)
    {
        StructuredArchive.EnterSlot(*this);
        StructuredArchive.InnerAr << Value;
        StructuredArchive.LeaveSlot();
    }

    void FArchiveSlot::Serialize(uint16& Value)
    {
        StructuredArchive.EnterSlot(*this);
        StructuredArchive.InnerAr << Value;
        StructuredArchive.LeaveSlot();
    }

    void FArchiveSlot::Serialize(uint32& Value)
    {
        StructuredArchive.EnterSlot(*this);
        StructuredArchive.InnerAr << Value;
        StructuredArchive.LeaveSlot();
    }

    void FArchiveSlot::Serialize(uint64& Value)
    {
        StructuredArchive.EnterSlot(*this);
        StructuredArchive.InnerAr << Value;
        StructuredArchive.LeaveSlot();
    }

    void FArchiveSlot::Serialize(int8& Value)
    {
        StructuredArchive.EnterSlot(*this);
        StructuredArchive.InnerAr << Value;
        StructuredArchive.LeaveSlot();
    }

    void FArchiveSlot::Serialize(int16& Value)
    {
        StructuredArchive.EnterSlot(*this);
        StructuredArchive.InnerAr << Value;
        StructuredArchive.LeaveSlot();
    }

    void FArchiveSlot::Serialize(int32& Value)
    {
        StructuredArchive.EnterSlot(*this);
        StructuredArchive.InnerAr << Value;
        StructuredArchive.LeaveSlot();
    }

    void FArchiveSlot::Serialize(int64& Value)
    {
        StructuredArchive.EnterSlot(*this);
        StructuredArchive.InnerAr << Value;
        StructuredArchive.LeaveSlot();
    }

    void FArchiveSlot::Serialize(float& Value)
    {
        StructuredArchive.EnterSlot(*this);
        StructuredArchive.InnerAr << Value;
        StructuredArchive.LeaveSlot();
    }

    void FArchiveSlot::Serialize(double& Value)
    {
        StructuredArchive.EnterSlot(*this);
        StructuredArchive.InnerAr << Value;
        StructuredArchive.LeaveSlot();
    }

    void FArchiveSlot::Serialize(bool& Value)
    {
        StructuredArchive.EnterSlot(*this);
        StructuredArchive.InnerAr << Value;
        StructuredArchive.LeaveSlot();
    }

    void FArchiveSlot::Serialize(FString& Value)
    {
        StructuredArchive.EnterSlot(*this);
        StructuredArchive.InnerAr << Value;
        StructuredArchive.LeaveSlot();
    }

    void FArchiveSlot::Serialize(FName& Value)
    {
        StructuredArchive.EnterSlot(*this);
        StructuredArchive.InnerAr << Value;
        StructuredArchive.LeaveSlot();
    }

    void FArchiveSlot::Serialize(CObject*& Value)
    {
        StructuredArchive.EnterSlot(*this);
        StructuredArchive.InnerAr << Value;
        StructuredArchive.LeaveSlot();
    }

    void FArchiveSlot::Serialize(FObjectHandle& Value)
    {
        StructuredArchive.EnterSlot(*this);
        StructuredArchive.InnerAr << Value;
        StructuredArchive.LeaveSlot();
    }

    void FArchiveSlot::Serialize(void* Data, uint64 DataSize)
    {
        StructuredArchive.EnterSlot(*this);
        StructuredArchive.InnerAr.Serialize(Data, (int64)DataSize);
        StructuredArchive.LeaveSlot();
    }

    FArchiveSlot IStructuredArchive::Open()
    {
        Assert(RootElementID == 0)

        RootElementID = IDGenerator.Generate();
        CurrentScope.emplace_back(RootElementID, StructuredArchive::EElementType::Root);

        CurrentSlotID = IDGenerator.Generate();
        
        return FArchiveSlot(*this, 0, CurrentSlotID);
        
    }

    void IStructuredArchive::Close()
    {
        
    }

    void IStructuredArchive::SetScope(FSlotPosition Slot)
    {
        Assert(Slot.Depth < CurrentScope.size() && CurrentScope[Slot.ID].ID == Slot.ID)

        CurrentScope.erase(CurrentScope.begin() + Slot.ID + 1, CurrentScope.end());
    }

    int32 IStructuredArchive::EnterSlotAsType(FSlotPosition Slot, StructuredArchive::EElementType Type)
    {
        EnterSlot(Slot, Type == StructuredArchive::EElementType::AttributedValue);

        int32 NewSlotDepth = Slot.Depth + 1;

        if (NewSlotDepth < CurrentScope.size() && CurrentScope[NewSlotDepth].Type == StructuredArchive::EElementType::AttributedValue)
        {
            
        }

        return 0;
    }

    void IStructuredArchive::EnterSlot(FSlotPosition Slot, bool bEnteringAttributedValue)
    {
    }

    FBinaryStructuredArchive::FBinaryStructuredArchive(FArchive& InAr)
        : IStructuredArchive(InAr)
    {
    }
    
    //------------------------------------------------------------------------------------------------------
}
