#include "Memory.h"

namespace Lumina
{
    void Memory::Initialize()
    {
        LUMINA_PROFILE_SCOPE();

        if (!GIsMemorySystemInitialized)
        {
#if defined(LUMINA_RPMALLOC)

            Memzero(&GrpmallocConfig, sizeof(rpmalloc_config_t));
            GrpmallocConfig.error_callback = CustomAssert;
            GrpmallocConfig.enable_huge_pages = true;
            
        
            rpmalloc_initialize_config(&GrpmallocConfig);

            GIsMemorySystemInitialized = true;
#endif
            std::cout << "[Lumina] - Memory System Initialized\n";
        }
    }
    
    void Memory::Shutdown()
    {
        LUMINA_PROFILE_SCOPE();
#if defined(LUMINA_RPMALLOC)

        rpmalloc_global_statistics_t stats;
        rpmalloc_global_statistics(&stats);
        
        std::cout << "[Lumina] - Memory System Shutdown.\n";

        rpmalloc_finalize();
#endif
        GIsMemorySystemInitialized = false;
        GIsMemorySystemShutdown = true;
    }

    void Memory::InitializeThreadHeap()
    {
#if defined(LUMINA_RPMALLOC)
        rpmalloc_thread_initialize();
#endif
    }

    SIZE_T Memory::GetActualAlignment(size_t size, size_t alignment)
    {
        // If alignment is 0 (DEFAULT_ALIGNMENT), pick based on size:
        // 8-byte alignment for small blocks (<16), otherwise 16-byte
        if (alignment == DEFAULT_ALIGNMENT)
        {
            return (size < 16) ? 8 : 16;
        }

        // Ensure minimum alignment rules
        SIZE_T DefaultAlignment = (size < 16) ? 8 : 16;
        SIZE_T Align = (alignment < DefaultAlignment) ? DefaultAlignment : (alignment);

        return Align;
    }

    void Memory::Memcpy(void* Destination, void* Source, uint64 SrcSize)
    {
        memcpy(Destination, Source, SrcSize);
        Assert(Destination != nullptr)
    }

    void Memory::Memcpy(void* Destination, const void* Source, uint64 SrcSize)
    {
        memcpy(Destination, Source, SrcSize);
        Assert(Destination != nullptr)
    }

    void* Memory::Malloc(size_t size, size_t alignment)
    {
#if defined(LUMINA_RPMALLOC)
        if(!GIsMemorySystemInitialized)
        {
            Initialize();
        }

        SIZE_T ActualAlignment = GetActualAlignment(size, alignment);
        void* pMemory = rpaligned_alloc(ActualAlignment, size);
        
        LUMINA_PROFILE_ALLOC(pMemory, size);
#else
        SIZE_T ActualAlignment = GetActualAlignment(size, alignment);
        void* pMemory = _aligned_malloc(size, ActualAlignment);
        Assert(IsAligned(pMemory, ActualAlignment))
        LUMINA_PROFILE_ALLOC(pMemory, size);
#endif

        return pMemory;
    }

    void* Memory::Realloc(void* Memory, size_t NewSize, size_t OriginalAlignment)
    {
        SIZE_T ActualAlignment = GetActualAlignment(NewSize, OriginalAlignment);
        
#if LUMINA_RPMALLOC
        void* pReallocatedMemory = rpaligned_realloc(Memory, ActualAlignment, NewSize, 0, 0);
        Assert(pReallocatedMemory != nullptr)

#else
        void* pReallocatedMemory = _aligned_realloc(Memory, NewSize, ActualAlignment);
        Assert(pReallocatedMemory != nullptr)
#endif
        
        return pReallocatedMemory;
        
    }

    void Memory::Free(void*& Memory)
    {
#if defined(LUMINA_RPMALLOC)
        LUMINA_PROFILE_FREE(Memory);
        rpfree(Memory);
        Memory = nullptr;
#else
        LUMINA_PROFILE_FREE(Memory);
        _aligned_free(Memory);
        Memory = nullptr;
#endif
    }
}
