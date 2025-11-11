#pragma once
#include "EASTL/internal/atomic/atomic.h"


namespace Lumina
{
    template<typename T>
    using TAtomic = eastl::atomic<T>;

    namespace Atomic
    {
        constexpr auto MemoryOrderRelaxed       = eastl::memory_order_relaxed;
        constexpr auto MemoryOrderReadDepends   = eastl::memory_order_read_depends;
        constexpr auto MemoryOrderAcquire       = eastl::memory_order_acquire;
        constexpr auto MemoryOrderRelease       = eastl::memory_order_release;
        constexpr auto MemoryOrderAcqRel        = eastl::memory_order_acq_rel;
        constexpr auto MemoryOrderSeqCst        = eastl::memory_order_seq_cst;
    }
}
