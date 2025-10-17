#pragma once
#include "Containers/Array.h"
#include "Core/Assertions/Assert.h"
#include "Core/Serialization/Archiver.h"
#include "Core/Templates/Optional.h"

namespace Lumina
{
    class FName;
    class FArchiveRecord;

    
    namespace StructuredArchive
    {
        using FSlotID = uint64;

        enum class EElementType : uint8
	    {
	    	Root,
	    	Record,
	    	Array,
	    	Stream,
	    	Map,
	    	AttributedValue,
	    };
        
        template<typename T>
        class TNamedValue
        {
            TNamedValue(FName InName, T& InValue)
                : Name(InName)
                , Value(InValue)
            {}
            
            FName Name;
            T& Value;
        };
    }
    
    class FSlotPosition
    {
    public:

        FSlotPosition() = default;

        FSlotPosition(uint32 InDepth, StructuredArchive::FSlotID InID = 0)
            : Depth(InDepth)
            , ID(InID)
        {}

        uint32 Depth = 0;
        StructuredArchive::FSlotID ID      = 0;
        
    };

    class FArchiveSlot : protected FSlotPosition
    {
        friend class IStructuredArchive;

    public:

        FArchiveSlot(IStructuredArchive& InAr, uint32 InDepth, StructuredArchive::FSlotID InID)
            : FSlotPosition(InDepth, InID)
            , StructuredArchive(InAr)
        {}

        LUMINA_API FArchiveRecord EnterRecord();
        
        void Serialize(uint8& Value);
        void Serialize(uint16& Value);
        void Serialize(uint32& Value);
        void Serialize(uint64& Value);
        void Serialize(int8& Value);
        void Serialize(int16& Value);
        void Serialize(int32& Value);
        void Serialize(int64& Value);
        void Serialize(float& Value);
        void Serialize(double& Value);
        void Serialize(bool& Value);
        void Serialize(FString& Value);
        void Serialize(FName& Value);
        void Serialize(CObject*& Value);
        void Serialize(FObjectHandle& Value);
        void Serialize(void* Data, uint64 DataSize);

        template<typename T>
        FORCEINLINE void operator << (StructuredArchive::TNamedValue<T> Item)
        {
            
        }

        template<typename T>
        void Serialize(TVector<T>& Value);
        
    protected:

        IStructuredArchive& StructuredArchive;
    };

    class FArchiveRecord : protected FSlotPosition
    {
        
    };
    

    class IStructuredArchive
    {
        friend class FArchiveSlot;
    
    public:
        using FSlot = FArchiveSlot;

        struct FIDGenerator
        {
            uint64 Generate()
            {
                return NextID++;
            }
            
            uint64 NextID = 1;
        };

        struct FElement
        {
            StructuredArchive::FSlotID ID;
            StructuredArchive::EElementType Type;

            FElement(StructuredArchive::FSlotID InID, StructuredArchive::EElementType InType)
                : ID(InID)
                , Type(InType)
            {}
            
        };

        virtual ~IStructuredArchive() = default;

        IStructuredArchive(FArchive& InInnerAr)
            : RootElementID(0)
            , CurrentSlotID(0)
            , InnerAr(InInnerAr)
        {
        }
        

        /** Begins writing an archive at the root slot */
        LUMINA_API FArchiveSlot Open();

        /** Flushes any remaining scope and closes the archive */
        LUMINA_API void Close();

        /** Switches the scope to the given slot */
        LUMINA_API void SetScope(FSlotPosition Slot);

        /** Enters the current slot, adding an element onto the stack. */
        LUMINA_API int32 EnterSlotAsType(FSlotPosition Slot, StructuredArchive::EElementType Type);

        /** Enters the current slot for serializing a value */
        virtual void EnterSlot(FSlotPosition Slot, bool bEnteringAttributedValue = false);

        
        virtual void EnterRecord() = 0;
        virtual void LeaveRecord() = 0;
    
        virtual void LeaveSlot() = 0;

        NODISCARD virtual FArchiveSlot EnterField(FName FieldName) = 0;
        virtual void LeaveField() = 0;

        FArchive& GetInnerAr() const { return InnerAr; }
        bool IsLoading() const { return InnerAr.IsReading(); }
        bool IsSaving() const { return InnerAr.IsWriting(); }

    protected:

        TFixedVector<FElement, 32>      CurrentScope;
        FIDGenerator                    IDGenerator;
        StructuredArchive::FSlotID      RootElementID;
        StructuredArchive::FSlotID      CurrentSlotID;
        FArchive&                       InnerAr;
    };

    
    class FBinaryStructuredArchive final : public IStructuredArchive
    {
    public:
        LUMINA_API FBinaryStructuredArchive(FArchive& InAr);
    
    private:

    };


    template <typename T>
    void FArchiveSlot::Serialize(TVector<T>& Value)
    {
        StructuredArchive.InnerAr << Value;
    }

}
