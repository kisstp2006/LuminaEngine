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
        
        CObjectBase* Object = nullptr;
        eastl::atomic<uint32> Generation{0};
        eastl::atomic<uint32> StrongRefCount{0};
        eastl::atomic<uint32> WeakRefCount{0};
        
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

        void AddStrongRef()
        {
            StrongRefCount.fetch_add(1, eastl::memory_order_relaxed);
        }

        uint32 ReleaseStrongRef()
        {
            uint32 PrevCount = StrongRefCount.fetch_sub(1, eastl::memory_order_acq_rel);
            return PrevCount - 1;
        }

        void AddWeakRef()
        {
            WeakRefCount.fetch_add(1, eastl::memory_order_relaxed);
        }

        void ReleaseWeakRef()
        {
            WeakRefCount.fetch_sub(1, eastl::memory_order_relaxed);
        }

        int32 GetStrongRefCount() const
        {
            return StrongRefCount.load(eastl::memory_order_relaxed);
        }

        int32 GetWeakRefCount() const
        {
            return WeakRefCount.load(eastl::memory_order_relaxed);
        }

        int32 GetGeneration() const
        {
            return Generation.load(eastl::memory_order_acquire);
        }

        void IncrementGeneration()
        {
            Generation.fetch_add(1, eastl::memory_order_release);
        }

        bool IsReferenced() const
        {
            return StrongRefCount.load(eastl::memory_order_relaxed) > 0;
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
    
        FMutex AllocationMutex;
    
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
        eastl::atomic<uint32> NumAliveObjects{0};
        
        mutable FMutex Mutex;
    
        bool bInitialized = false;
        bool bShuttingDown = false;
    
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
            bShuttingDown = true;
            
            ForEachObject([](CObjectBase* Object, int32)
            {
                Object->OnDestroy();
                Memory::Delete(Object);
            });
            
            ChunkedArray.Shutdown();
            FreeIndices.clear();
            NumAliveObjects.store(0, eastl::memory_order_relaxed);
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
                Item->Generation.store(Generation, eastl::memory_order_release);
                Item->SetObj(Object);
    
                ChunkedArray.IncrementElementCount();
            }
    
            NumAliveObjects.fetch_add(1, eastl::memory_order_relaxed);

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
    
            NumAliveObjects.fetch_sub(1, eastl::memory_order_relaxed);
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

        // Add a strong reference to an object by pointer
        void AddStrongRef(CObjectBase* Object)
        {
            if (Object)
            {
                FCObjectItem* Item = ChunkedArray.GetItem(Object->GetInternalIndex());
                if (Item)
                {
                    Item->AddStrongRef();
                }
            }
        }

        // Release a strong reference by pointer
        // Returns true if object was deleted.
        bool ReleaseStrongRef(CObjectBase* Object)
        {
            // Shutting down means we're manually destroying every object, so we don't want to do anything else.
            if (bShuttingDown)
            {
                return false;
            }
            
            if (Object)
            {
                FCObjectItem* Item = ChunkedArray.GetItem(Object->GetInternalIndex());
                if (Item)
                {
                    uint32 NewCount = Item->ReleaseStrongRef();
                    if (NewCount == 0)
                    {
                        Object->ConditionalBeginDestroy();
                        return true;
                    }
                }
            }
            return false;
        }
    
        void AddStrongRefByIndex(int32 Index)
        {
            FCObjectItem* Item = ChunkedArray.GetItem(Index);
            if (Item)
            {
                Item->AddStrongRef();
            }
        }
    
        bool ReleaseStrongRefByIndex(int32 Index)
        {
            FCObjectItem* Item = ChunkedArray.GetItem(Index);
            if (Item)
            {
                uint32 NewCount = Item->ReleaseStrongRef();
                return NewCount == 0 && Item->GetObj() != nullptr;
            }
            return false;
        }

        void AddWeakRefByIndex(int32 Index)
        {
            FCObjectItem* Item = ChunkedArray.GetItem(Index);
            if (Item)
            {
                Item->AddWeakRef();
            }
        }

        void ReleaseWeakRefByIndex(int32 Index)
        {
            FCObjectItem* Item = ChunkedArray.GetItem(Index);
            if (Item)
            {
                Item->ReleaseWeakRef();
            }
        }

        bool IsReferencedByIndex(int32 Index) const
        {
            const FCObjectItem* Item = ChunkedArray.GetItem(Index);
            return Item && Item->IsReferenced();
        }

        int32 GetStrongRefCountByIndex(int32 Index) const
        {
            const FCObjectItem* Item = ChunkedArray.GetItem(Index);
            return Item ? Item->GetStrongRefCount() : 0;
        }
    
        int32 GetNumAliveObjects() const
        {
            return NumAliveObjects.load(eastl::memory_order_relaxed);
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
