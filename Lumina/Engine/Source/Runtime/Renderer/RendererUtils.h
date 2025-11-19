#pragma once

#include <algorithm>
#include "Platform/GenericPlatform.h"

namespace Lumina::RenderUtils
{
    inline uint32 CalculateMipCount(uint32 Width, uint32 Height)
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

    inline uint32 GetMipDim(uint32 BaseWidth, uint32 Level)
    {
        return std::max(1u, BaseWidth >> Level);
    }

    inline uint32 GetGroupCount(uint32 ThreadCount, uint32 LocalSize)
    {
        return (ThreadCount + LocalSize - 1) / LocalSize;
    };
}
