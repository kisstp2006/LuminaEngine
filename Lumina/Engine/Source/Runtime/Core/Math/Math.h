#pragma once

#include <random>
#include <EASTL/algorithm.h>

#include "glm/glm.hpp"

namespace Lumina
{
    namespace Math
    {

        template<typename T>
        FORCEINLINE T Max(const T& First, const T& Second)
        {
            return glm::max(First, Second);
        }

        template<typename T>
        FORCEINLINE T Min(const T& First, const T& Second)
        {
            return glm::min(First, Second);
        }

        template<typename T>
        FORCEINLINE T Lerp(const T& A, const T& B, float Alpha)
        {
            return A + (B - A) * Alpha;
        }        
        template<typename T>
        requires(eastl::is_integral_v<T> && eastl::is_unsigned_v<T> && (sizeof(T) <= 4))
        FORCEINLINE T RandRange(T First, T Second)
        {
            if (First > Second)
            {
                eastl::swap(First, Second);
            }

            thread_local std::mt19937_64 rng(std::random_device{}());
            eastl::uniform_int_distribution<T> dist(First, Second);
    
            return dist(rng);
        }
    }
}
