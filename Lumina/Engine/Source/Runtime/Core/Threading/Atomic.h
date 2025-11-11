#pragma once
#include "EASTL/internal/atomic/atomic.h"


namespace Lumina
{
    template<typename T>
    using TAtomic = eastl::atomic<T>;
}
