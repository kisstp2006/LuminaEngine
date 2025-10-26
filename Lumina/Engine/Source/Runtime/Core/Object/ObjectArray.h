#pragma once
#include "ObjectBase.h"
#include "ObjectHandle.h"
#include "Containers/Array.h"
#include "Core/Profiler/Profile.h"
#include "Core/Threading/Thread.h"
#include "Platform/GenericPlatform.h"

namespace Lumina
{
    class CObjectBase;
}

namespace Lumina
{

    struct FCObjectItem
    {
        FCObjectItem() = default;
        
        CObjectBase*        Object = nullptr;

        std::atomic_int32_t Generation{0};
        std::atomic_int32_t RefCount{0};

        
        // Non-copyable
        FCObjectItem(FCObjectItem&&) = delete;
        FCObjectItem(const FCObjectItem&) = delete;
        FCObjectItem& operator=(FCObjectItem&&) = delete;
        FCObjectItem& operator=(const FCObjectItem&) = delete;

        FORCEINLINE CObjectBase* GetObj() const
        {
            return Object;
        }

        FORCEINLINE void SetObj(CObjectBase* InObject)
        {
            Object = InObject;
        }

        void AddRef()
        {
            RefCount.fetch_add(1, std::memory_order_relaxed);
        }

        void ReleaseRef()
        {
            RefCount.fetch_sub(1, std::memory_order_relaxed);
        }

        int32 GetRefCount() const
        {
            return RefCount.load(std::memory_order_relaxed);
        }

        int32 GetGeneration() const
        {
            return Generation.load(std::memory_order_acquire);
        }

        void IncrementGeneration()
        {
            Generation.fetch_add(1, std::memory_order_release);
        }
    };


    class FChunkedFixedCObjectArray
    {
    public:
        
        static constexpr int32 NumElementsPerChunk = 64 * 1024;
    
    private:
        
        // Array of chunk pointers.
        FCObjectItem** Objects = nullptr;
    
        // Pre-allocated object storage.
        FCObjectItem* PreAllocatedObjects = nullptr;
    
        int32 MaxElements = 0;
        int32 NumElements = 0;
        int32 MaxChunks = 0;
        int32 NumChunks = 0;
    
        std::mutex AllocationMutex;
    
    public:
        
        FChunkedFixedCObjectArray()
        {
        }
    
        ~FChunkedFixedCObjectArray()
        {
            Shutdown();
        }
    
        // Initialize with maximum capacity
        void Initialize(int32 InMaxElements)
        {
            assert(Objects == nullptr && "Already initialized!");
            assert(InMaxElements > 0);
    
            MaxElements = InMaxElements;
            MaxChunks = (InMaxElements + NumElementsPerChunk - 1) / NumElementsPerChunk;
            NumChunks = 0;
            NumElements = 0;
    
            // Allocate array of chunk pointers (never reallocates)
            Objects = new FCObjectItem*[MaxChunks];
            
            // Initialize all chunk pointers to nullptr
            for (int32 i = 0; i < MaxChunks; ++i)
            {
                Objects[i] = nullptr;
            }
            
            PreAllocateAllChunks();
        }
    
        void Shutdown()
        {
            if (Objects)
            {
                // Free all allocated chunks
                for (int32 i = 0; i < NumChunks; ++i)
                {
                    if (Objects[i])
                    {
                        delete[] Objects[i];
                        Objects[i] = nullptr;
                    }
                }
    
                delete[] Objects;
                Objects = nullptr;
            }
    
            PreAllocatedObjects = nullptr;
            MaxElements = 0;
            NumElements = 0;
            MaxChunks = 0;
            NumChunks = 0;
        }
    
        // Pre-allocate all chunks at initialization to avoid any runtime allocation
        void PreAllocateAllChunks()
        {
            std::lock_guard<std::mutex> Lock(AllocationMutex);
    
            for (int32 ChunkIndex = 0; ChunkIndex < MaxChunks; ++ChunkIndex)
            {
                if (!Objects[ChunkIndex])
                {
                    Objects[ChunkIndex] = new FCObjectItem[NumElementsPerChunk];
                    NumChunks = ChunkIndex + 1;
                }
            }
        }
    
        // Get item at index (const version)
        const FCObjectItem* GetItem(int32 Index) const
        {
            if (Index < 0 || Index >= MaxElements)
            {
                return nullptr;
            }
    
            const int32 ChunkIndex = Index / NumElementsPerChunk;
            const int32 SubIndex = Index % NumElementsPerChunk;
    
            // Check if chunk exists
            if (ChunkIndex >= NumChunks || !Objects[ChunkIndex])
            {
                return nullptr;
            }
    
            return &Objects[ChunkIndex][SubIndex];
        }
    
        // Get item at index (non-const version)
        FCObjectItem* GetItem(int32 Index)
        {
            if (Index < 0 || Index >= MaxElements)
            {
                return nullptr;
            }
    
            const int32 ChunkIndex = Index / NumElementsPerChunk;
            const int32 SubIndex = Index % NumElementsPerChunk;
    
            // Expand chunks if needed (should rarely happen if pre-allocated)
            if (ChunkIndex >= NumChunks)
            {
                ExpandChunksToIndex(Index);
            }
    
            return &Objects[ChunkIndex][SubIndex];
        }
    
        int32 GetMaxElements() const { return MaxElements; }
        int32 GetNumElements() const { return NumElements; }
        int32 GetNumChunks() const { return NumChunks; }
    
        // Increment element count (called by FCObjectArray on allocation)
        void IncrementElementCount()
        {
            ++NumElements;
        }
    
        // Decrement element count (called by FCObjectArray on deallocation)
        void DecrementElementCount()
        {
            --NumElements;
        }
    
    private:
        void ExpandChunksToIndex(int32 Index)
        {
            const int32 ChunkIndex = Index / NumElementsPerChunk;
    
            if (ChunkIndex >= MaxChunks)
            {
                assert(false && "Index exceeds maximum capacity!");
                return;
            }
    
            std::lock_guard<std::mutex> Lock(AllocationMutex);
    
            // Allocate chunks up to and including the required chunk
            for (int32 i = NumChunks; i <= ChunkIndex; ++i)
            {
                if (!Objects[i])
                {
                    Objects[i] = new FCObjectItem[NumElementsPerChunk];
                }
            }
    
            NumChunks = ChunkIndex + 1;
        }
    };
        
    class FCObjectArray
    {
    private:
        
        FChunkedFixedCObjectArray ChunkedArray;
        
        TVector<int32> FreeIndices;
        std::atomic_int32_t NumAliveObjects{0};
        
        mutable FMutex Mutex;
    
        bool bInitialized = false;
    
    public:
        FCObjectArray() = default;
        ~FCObjectArray() = default;
    
        // Initialize the object array with maximum capacity
        void AllocateObjectPool(int32 InMaxCObjects)
        {
            assert(!bInitialized && "Object pool already allocated!");
    
            ChunkedArray.Initialize(InMaxCObjects);
            
            FreeIndices.reserve(InMaxCObjects / 4);
    
            bInitialized = true;
        }
    
        void Shutdown()
        {
            ChunkedArray.Shutdown();
            FreeIndices.clear();
            NumAliveObjects.store(0, std::memory_order_relaxed);
            bInitialized = false;
        }
    
        // Allocate a slot for an object and return a handle
        FObjectHandle AllocateObject(CObjectBase* Object)
        {
            assert(bInitialized && "Object pool not initialized!");
            assert(Object != nullptr);
    
            FScopeLock Lock(Mutex);
    
            int32 Index;
            int32 Generation;
    
            // Try to reuse a free slot
            if (!FreeIndices.empty())
            {
                Index = FreeIndices.back();
                FreeIndices.pop_back();
    
                FCObjectItem* Item = ChunkedArray.GetItem(Index);
                assert(Item != nullptr);
                assert(Item->GetObj() == nullptr);
    
                // Increment generation for reused slot
                Item->IncrementGeneration();
                Generation = Item->GetGeneration();
                
                Item->SetObj(Object);
            }
            else
            {
                // Allocate new slot
                Index = ChunkedArray.GetNumElements();
    
                if (Index >= ChunkedArray.GetMaxElements())
                {
                    assert(false && "Object pool capacity exceeded!");
                    return FObjectHandle();
                }
    
                FCObjectItem* Item = ChunkedArray.GetItem(Index);
                assert(Item != nullptr);
    
                Generation = 1;
                Item->Generation.store(Generation, std::memory_order_release);
                Item->SetObj(Object);
    
                ChunkedArray.IncrementElementCount();
            }
    
            NumAliveObjects.fetch_add(1, std::memory_order_relaxed);

            return FObjectHandle(Index, Generation);
        }
    
        // Deallocate an object slot
        void DeallocateObject(int32 Index)
        {
            assert(bInitialized && "Object pool not initialized!");
    
            FScopeLock Lock(Mutex);
    
            FCObjectItem* Item = ChunkedArray.GetItem(Index);
            assert(Item != nullptr);
            assert(Item->GetObj() != nullptr);
    
            Item->SetObj(nullptr);
    
            Item->IncrementGeneration();
    
            FreeIndices.push_back(Index);
    
            NumAliveObjects.fetch_sub(1, std::memory_order_relaxed);
        }
    
        // Resolve a handle to an object pointer
        CObjectBase* ResolveHandle(const FObjectHandle& Handle) const
        {
            if (!Handle.IsValid())
            {
                return nullptr;
            }
    
            const FCObjectItem* Item = ChunkedArray.GetItem(Handle.Index);
            if (!Item)
            {
                return nullptr;
            }
    
            // Stable snapshot pattern: gen1 -> obj -> gen2
            const int32 Gen1 = Item->GetGeneration();
            if (Gen1 != Handle.Generation)
            {
                return nullptr;
            }
    
            CObjectBase* Obj = Item->GetObj();
            if (!Obj)
            {
                return nullptr;
            }
    
            const int32 Gen2 = Item->GetGeneration();
            if (Gen2 != Gen1)
            {
                return nullptr;
            }
    
            return Obj;
        }
    
        CObjectBase* GetObjectByIndex(int32 Index) const
        {
            const FCObjectItem* Item = ChunkedArray.GetItem(Index);
            return Item ? Item->GetObj() : nullptr;
        }

        FObjectHandle GetHandleByObject(const CObjectBase* Object) const
        {
            return GetHandleByIndex(Object->GetInternalIndex());
        }
        
    
        // Get handle from object index and current generation
        FObjectHandle GetHandleByIndex(int32 Index) const
        {
            const FCObjectItem* Item = ChunkedArray.GetItem(Index);
            if (!Item || !Item->GetObj())
            {
                return FObjectHandle();
            }
    
            return FObjectHandle(Index, Item->GetGeneration());
        }
    
        // Add a reference to an object at index
        void AddRefByIndex(int32 Index)
        {
            FCObjectItem* Item = ChunkedArray.GetItem(Index);
            if (Item)
            {
                Item->AddRef();
            }
        }
    
        // Release a reference to an object at index
        void ReleaseRefByIndex(int32 Index)
        {
            FCObjectItem* Item = ChunkedArray.GetItem(Index);
            if (Item)
            {
                Item->ReleaseRef();
            }
        }
    
        int32 GetNumAliveObjects() const
        {
            return NumAliveObjects.load(std::memory_order_relaxed);
        }
    
        int32 GetMaxObjects() const
        {
            return ChunkedArray.GetMaxElements();
        }
    
        template<typename Func>
        requires(eastl::is_invocable_v<Func, CObjectBase*, int32>)
        void ForEachObject(Func&& Function) const
        {
            const int32 MaxElements = ChunkedArray.GetNumElements();
            
            for (int32 i = 0; i < MaxElements; ++i)
            {
                const FCObjectItem* Item = ChunkedArray.GetItem(i);
                if (Item && Item->GetObj())
                {
                    std::forward<Func>(Function)(Item->GetObj(), i);
                }
            }
        }
    };
    
    // Global object array instance
    extern LUMINA_API FCObjectArray GObjectArray;
    
}
