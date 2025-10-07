#pragma once
#include "glm/glm.hpp"
#include "Platform/Platform.h"
#include "Core/Profiler/Profile.h"
#include <limits>

namespace Lumina
{
    struct FAABB
    {
        glm::vec3 Min;
        glm::vec3 Max;
        
        FAABB()
            : Min(0.0f), Max(0.0f)
        {}

        FAABB(const glm::vec3& InMin, const glm::vec3& InMax)
            : Min(InMin), Max(InMax)
        {}

        NODISCARD glm::vec3 GetSize() const { return Max - Min; }
        NODISCARD glm::vec3 GetCenter() const { return Min + GetSize() * 0.5f; }

        NODISCARD FAABB ToWorld(const glm::mat4& World) const
        {
            glm::vec3 NewMin(std::numeric_limits<float>::max());
            glm::vec3 NewMax(-std::numeric_limits<float>::max());

            for (int i = 0; i < 8; i++)
            {
                glm::vec3 corner = glm::vec3
                (
                    (i & 1) ? Max.x : Min.x,
                    (i & 2) ? Max.y : Min.y,
                    (i & 4) ? Max.z : Min.z
                );

                glm::vec4 transformed = World * glm::vec4(corner, 1.0f);
                glm::vec3 p = glm::vec3(transformed) / transformed.w;

                NewMin = glm::min(NewMin, p);
                NewMax = glm::max(NewMax, p);
            }

            return FAABB(NewMin, NewMax);
        }
    };
}