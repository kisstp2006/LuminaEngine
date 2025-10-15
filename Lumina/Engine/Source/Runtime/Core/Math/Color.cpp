#include "Color.h"

#include "glm/glm.hpp"

namespace Lumina
{
    constexpr FColor FColor::Red        = FColor(1.0f, 0.0f, 0.0f);
    constexpr FColor FColor::Green      = FColor(0.0f, 1.0f, 0.0f);
    constexpr FColor FColor::Blue       = FColor(0.0f, 0.0f, 1.0f);
    constexpr FColor FColor::Yellow     = FColor(1.0f, 1.0f, 0.0f);
    constexpr FColor FColor::White      = FColor(1.0f, 1.0f, 1.0f);
    constexpr FColor FColor::Black      = FColor(0.0f, 0.0f, 0.0f);
}
