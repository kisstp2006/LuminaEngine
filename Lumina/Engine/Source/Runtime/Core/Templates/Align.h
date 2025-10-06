#pragma once
#include "Platform/GenericPlatform.h"


namespace Lumina
{
    template <typename T>
    requires (eastl::is_integral_v<T> || eastl::is_pointer_v<T>)
    constexpr T Align(T Val, uint64 Alignment)
    {
        return (Val + Alignment - 1) & ~(Alignment - 1);
    }
}
