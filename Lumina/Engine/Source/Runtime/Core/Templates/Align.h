#pragma once


namespace Lumina
{
    template <typename T>
    requires (eastl::is_integral_v<T> || eastl::is_pointer_v<T>)
    constexpr T Align(T Val, T Alignment)
    {
        return static_cast<T>((Val + Alignment - 1) & ~(Alignment - 1));
    }
}
