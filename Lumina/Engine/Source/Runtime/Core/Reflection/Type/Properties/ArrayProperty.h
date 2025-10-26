#pragma once
#include "Core/Reflection/Type/LuminaTypes.h"
#include "Lumina.h"

namespace Lumina
{

    
    class FArrayProperty : public FProperty
    {
    public:

        
        FArrayProperty(const FFieldOwner& InOwner, const FArrayPropertyParams* Params)
            :FProperty(InOwner, Params)
        {
            PushBackFn = Params->PushBackFn;
            GetNumFn = Params->GetNumFn;
            RemoveAtFn = Params->RemoveAtFn;
            ClearFn = Params->ClearFn;
            GetAtFn = Params->GetAtFn;
        }
        
        ~FArrayProperty() override = default;

        void AddProperty(FProperty* Property) override { Inner = Property; }

        void Serialize(FArchive& Ar, void* Value) override;
        void SerializeItem(IStructuredArchive::FSlot Slot, void* Value, void const* Defaults) override;

        FProperty* GetInternalProperty() const { return Inner; }
        
        SIZE_T GetNum(const void* InContainer) const
        {
            return GetNumFn(InContainer);
        }

        void PushBack(void* InContainer, const void* InValue) const
        {
            PushBackFn(InContainer, InValue);
        }

        void RemoveAt(void* InContainer, size_t Index) const
        {
            RemoveAtFn(InContainer, Index);
        }

        void Clear(void* InContainer) const
        {
            ClearFn(InContainer);
        }

        void* GetAt(void* InContainer, size_t Index) const
        {
            return GetAtFn(InContainer, Index);
        }

        template<typename T = void, typename TFunc>
        void ForEach(void* InContainer, TFunc&& Func) const
        {
            SIZE_T Num = GetNum(InContainer);
            for (SIZE_T i = 0; i < Num; ++i)
            {
                if constexpr (std::is_same_v<T, void>)
                {
                    void* Elem = GetAt(InContainer, i);
                    Func(Elem, i);
                }
                else
                {
                    T* Elem = static_cast<T*>(GetAt(InContainer, i));
                    Func(Elem, i);
                }
            }
        }
        
    private:

        ArrayPushBackPtr    PushBackFn;
        ArrayGetNumPtr      GetNumFn;
        ArrayRemoveAtPtr    RemoveAtFn;
        ArrayClearPtr       ClearFn;
        ArrayGetAtPtr       GetAtFn;
        FProperty* Inner = nullptr;
        
    };
}
