#pragma once

#include "Core/Math/Math.h"
#include "Events/Event.h"
#include "Containers/Function.h"

namespace Lumina
{
    struct FWindowSpecs
    {
        FString Title = "Lumina";
        glm::uvec2 Extent = glm::uvec2(0);
        bool bFullscreen = false;
    };
}
