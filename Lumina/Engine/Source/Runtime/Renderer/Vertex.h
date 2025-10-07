#pragma once

#include "Core/Math/Color.h"
#include "Core/Math/Math.h"
#include "Core/Templates/CanBulkSerialize.h"



namespace Lumina
{

    // Pack normalized normal into 10:10:10:2 format
    inline uint32 PackNormal(glm::vec3 normal)
    {
        int x = (int)(glm::clamp(normal.x, -1.0f, 1.0f) * 511.0f);
        int y = (int)(glm::clamp(normal.y, -1.0f, 1.0f) * 511.0f);
        int z = (int)(glm::clamp(normal.z, -1.0f, 1.0f) * 511.0f);
        return ((x & 0x3FF) << 0) | ((y & 0x3FF) << 10) | ((z & 0x3FF) << 20);
    }

    // Pack color into RGBA8
    inline uint32 PackColor(glm::vec4 color)
    {
        uint8 r = (uint8)(glm::clamp(color.r, 0.0f, 1.0f) * 255.0f);
        uint8 g = (uint8)(glm::clamp(color.g, 0.0f, 1.0f) * 255.0f);
        uint8 b = (uint8)(glm::clamp(color.b, 0.0f, 1.0f) * 255.0f);
        uint8 a = (uint8)(glm::clamp(color.a, 0.0f, 1.0f) * 255.0f);
        return (a << 24) | (b << 16) | (g << 8) | r;
    }
    
    struct FVertex
    {
        glm::vec3       Position;
        uint32          Normal;
        glm::u16vec2    UV;
        uint32          Color;

        
        friend FArchive& operator<<(FArchive& Ar, FVertex& Data)
        {
            Ar << Data.Position;
            Ar << Data.Normal;
            Ar << Data.UV;
            Ar << Data.Color;
            
            return Ar;
        }
    };

    struct FSimpleElementVertex
    {
        glm::vec4   Position;
        glm::vec4      Color;
    };

    static_assert(offsetof(FVertex, Position) == 0, "Required FVertex::Position to be the first member.");
    static_assert(std::is_trivially_copyable_v<FVertex>, "FVertex must be trivially copyable.");
    
    template<>
    struct TCanBulkSerialize<FVertex> { static constexpr bool Value = true; };

    template<>
    struct TCanBulkSerialize<FSimpleElementVertex> { static constexpr bool Value = true; };
    
}
