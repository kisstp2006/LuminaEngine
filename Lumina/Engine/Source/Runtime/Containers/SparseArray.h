#pragma once
#include "Array.h"


namespace Lumina
{
    /** A dynamic array that internally uses a free list, (non-contiguous). */
    template <typename T>
    class TSparseArray
    {
    public:
        using Index = size_t;
    
        Index Insert(const T& value)
        {
            if (!FreeList.empty())
            {
                Index idx = FreeList.back();
                FreeList.pop_back();
                Slots[idx].bOccupied = true;
                Slots[idx].Value = value;
                return idx;
            }
            
            Slots.emplace_back({true, value});
            return Slots.size() - 1;
        }
        
        void Erase(Index idx)
        {
            if (idx < Slots.size() && Slots[idx].bOccupied)
            {
                Slots[idx].bOccupied = false;
                FreeList.push_back(idx);
            }
        }
    
        T* Get(Index idx)
        {
            return (idx < Slots.size() && Slots[idx].bOccupied) ? &Slots[idx].Value : nullptr;
        }
    
        auto begin() { return Slots.begin(); }
        auto end()   { return Slots.end();   }

        T* operator [] (Index Index)
        {
            return Get(Index);
        }
        
    private:
        
        struct FSlot
        {
            uint8 bOccupied:1=0;
            T Value{};
        };
    
        TVector<FSlot>  Slots;
        TBitVector      FreeList;
    };
}
