#pragma once

#include "Containers/Array.h"
#include "glm/glm.hpp"
#include "Renderer/Vertex.h"


namespace Lumina
{
    inline void GenerateCube(TVector<FVertex>& OutVertices, TVector<uint32>& OutIndices)
    {
        const glm::vec3 normals[] =
        {
            { 0,  0,  1}, { 0,  0, -1}, {-1,  0,  0},
            { 1,  0,  0}, { 0,  1,  0}, { 0, -1,  0}
        };

        const glm::vec3 positions[24] =
        {
            // Front
            {-1, -1,  1}, {1, -1,  1}, {1, 1,  1}, {-1, 1,  1},
            // Back
            {1, -1, -1}, {-1, -1, -1}, {-1, 1, -1}, {1, 1, -1},
            // Left
            {-1, -1, -1}, {-1, -1, 1}, {-1, 1, 1}, {-1, 1, -1},
            // Right
            {1, -1, 1}, {1, -1, -1}, {1, 1, -1}, {1, 1, 1},
            // Top
            {-1, 1, 1}, {1, 1, 1}, {1, 1, -1}, {-1, 1, -1},
            // Bottom
            {-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1},
        };

        const glm::vec2 uvs[4] =
        {
            {0, 0}, {1, 0}, {1, 1}, {0, 1}
        };

        OutVertices.clear();
        OutIndices.clear();
        OutVertices.reserve(24);

        for (int face = 0; face < 6; ++face)
        {
            for (int i = 0; i < 4; ++i)
            {
                int idx = face * 4 + i;
            
                FVertex vertex;
                vertex.Position = positions[idx];
                vertex.Normal = PackNormal(normals[face]);
                vertex.UV.x = (uint16)(uvs[i].x * 65535.0f);
                vertex.UV.y = (uint16)(uvs[i].y * 65535.0f);
                vertex.Color = 0xFFFFFFFF; // White
            
                OutVertices.push_back(vertex);
            }

            uint32 base = face * 4;
            OutIndices.push_back(base + 0);
            OutIndices.push_back(base + 1);
            OutIndices.push_back(base + 2);
            OutIndices.push_back(base + 2);
            OutIndices.push_back(base + 3);
            OutIndices.push_back(base + 0);
        }
    }
}
