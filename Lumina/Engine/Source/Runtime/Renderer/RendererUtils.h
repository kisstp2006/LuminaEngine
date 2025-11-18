#pragma once

#include <algorithm>
#include "Platform/GenericPlatform.h"

namespace Lumina::RenderUtils
{
    static uint32 CalculateMipCount(uint32 Width, uint32 Height)
    {
        uint32 Levels = 1;
        while (Width > 1 || Height > 1)
        {
            Width = std::max(Width >> 1, 1u);
            Height = std::max(Height >> 1, 1u);
            ++Levels;
        }
        return Levels;
    }
    
}
