#include "Core/Object/Class.h"
#include "Core/Object/Field.h"
#include "Containers/Function.h"
#include "Core/Reflection/Type/LuminaTypes.h"
#include "Core/Reflection/Type/Properties/ArrayProperty.h"
#include "Core/Reflection/Type/Properties/PropertyTag.h"

IMPLEMENT_INTRINSIC_CLASS(CStruct, CField, LUMINA_API)

namespace Lumina
{

    void CStruct::SetSuperStruct(CStruct* InSuper)
    {
        SuperStruct = InSuper;
    }

    void CStruct::RegisterDependencies()
    {
        if (SuperStruct != nullptr)
        {
            SuperStruct->RegisterDependencies();
        }
    }

    bool CStruct::IsChildOf(const CStruct* Base) const
    {
        // Do *not* call IsChildOf with a nullptr. It is UB.
        Assert(this);
        
        if (Base == nullptr)
        {
            return false;
        }

        bool bOldResult = false;
        for (const CStruct* Temp = this; Temp; Temp = Temp->GetSuperStruct())
        {
            if (Temp == Base)
            {
                bOldResult = true;
                break;
            }
        }

        return bOldResult;
    }

    void CStruct::Link()
    {

        if (bLinked) return;
        bLinked = true;

        if (SuperStruct)
        { 
            SuperStruct->Link();
        }

        if (SuperStruct && SuperStruct->LinkedProperty)
        {
            FProperty* SuperProperty = SuperStruct->LinkedProperty;

            if (LinkedProperty == nullptr)
            {
                LinkedProperty = SuperProperty;
            }
            else
            {
                FProperty* Current = LinkedProperty;
                while (Current->Next != nullptr)
                {
                    Current = (FProperty*)Current->Next;
                }
                Current->Next = SuperProperty;
            }
        }
    }


    FProperty* CStruct::GetProperty(const FName& Name)
    {
        FProperty* Current = LinkedProperty;
        while (Current != nullptr)
        {
            if (Current->Name == Name)
            {
                return Current;
            }
            
            Current = (FProperty*)Current->Next;
        }
        return nullptr;
    }

    void CStruct::AddProperty(FProperty* Property)
    {
        if (LinkedProperty == nullptr)
        {
            LinkedProperty = Property;
        }
        else
        {
            FProperty* Current = LinkedProperty;
            while (Current->Next != nullptr)
            {
                Current = (FProperty*)Current->Next;
            }
            Current->Next = Property;
        }
        
        Property->Next = nullptr;
    }

    void CStruct::SerializeTaggedProperties(FArchive& Ar, void* Data)
    {
        if (Ar.IsWriting())
        {
            uint32 NumProperties = 0;
            int64 NumPropertiesWritePos = Ar.Tell();
            Ar << NumProperties;
        
            for (FProperty* Current = LinkedProperty; Current; Current = (FProperty*)Current->Next)
            {
                FPropertyTag PropertyTag;
                PropertyTag.Type = Current->GetTypeAsFName();
                PropertyTag.Name = Current->GetPropertyName();

                // Write a placeholder tag to measure its size
                int64 TagPosition = Ar.Tell();
                Ar << PropertyTag;
                int64 AfterTagPosition = Ar.Tell();
            
                PropertyTag.Offset = AfterTagPosition;
            
                void* ValuePtr = Current->GetValuePtr<void>(Data);
            
                if (dynamic_cast<FArrayProperty*>(Current))
                {
                    ValuePtr = Data;
                }
                
                Current->Serialize(Ar, ValuePtr);

                int64 DataEndPosition = Ar.Tell();
                PropertyTag.Size = (int32)(DataEndPosition - AfterTagPosition);
            
                // Go back and rewrite the tag with correct values
                Ar.Seek(TagPosition);
                Ar << PropertyTag;
            
                Ar.Seek(DataEndPosition);

                NumProperties++;
            }

            int64 Pos = Ar.Tell();
            Ar.Seek(NumPropertiesWritePos);
            Ar << NumProperties;
            Ar.Seek(Pos);

        }
        else if (Ar.IsReading())
        {
            uint32 NumProperties = 0;
            Ar << NumProperties;

            FProperty* Current = LinkedProperty;
            for (uint32 i = 0; i < NumProperties; ++i)
            {
                FPropertyTag Tag;
                Ar << Tag;
        
                int64 DataStartPos = Ar.Tell();
        
                FProperty* FoundProperty = nullptr;

                // First try for an O(n) search, as the order may still match.
                if (Current->GetTypeAsFName() == Tag.Type && Current->GetPropertyName() == Tag.Name)
                {
                    FoundProperty = Current;
                    Current = (FProperty*)Current->Next;
                }
        
                // Property was not found, fallback to an O(n^2) search, as it may have changed order.
                if (FoundProperty == nullptr)
                {
                    for (FProperty* Search = LinkedProperty; Search; Search = (FProperty*)Search->Next)
                    {
                        if (Search->GetTypeAsFName() == Tag.Type && Search->GetPropertyName() == Tag.Name)
                        {
                            FoundProperty = Search;
                            break;
                        }
                    }
                }
        
                if (FoundProperty)
                {
                    void* ValuePtr = FoundProperty->GetValuePtr<void>(Data);

                    // TODO, temp garbage.
                    if (dynamic_cast<FArrayProperty*>(FoundProperty))
                    {
                        ValuePtr = Data;
                    }
            
                    FoundProperty->Serialize(Ar, ValuePtr);
                }
                else
                {
                    // Property doesn't exist, skip it
                    LOG_WARN("Property '{}' of type '{}' not found in struct, skipping", Tag.Name.ToString(), Tag.Type.ToString());
                }
        
                // Always seek past this property's data to read the next tag
                Ar.Seek(DataStartPos + Tag.Size);
            }
            
        }
    }

    void CStruct::SerializeTaggedProperties(IStructuredArchive::FSlot Slot, void* Data)
    {
        
    }
}
