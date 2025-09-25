#pragma once

#include <glm/glm.hpp>

#include "Core/Math/Transform.h"
#include "Platform/GenericPlatform.h"
#include "Renderer/MeshData.h"
#include "Renderer/RenderResource.h"
#include "Renderer/RHIFwd.h"

#define MAX_LIGHTS 1024

namespace Lumina
{
    class FRenderScene;
}

namespace Lumina
{
    class CMaterialInterface;
    struct FVertex;
    class CMaterial;
    class CStaticMesh;
}

namespace Lumina
{
    enum class ESceneRenderGBuffer : uint8
    {
        RenderTarget,
        Albedo,
        Position,
        Normals,
        Material,
        Depth,
        SSAO,
    };
    
    struct FCameraData
    {
        glm::vec4 Location =    {};
        glm::mat4 View =        {};
        glm::mat4 InverseView = {};
        glm::mat4 Projection =  {};
        glm::mat4 InverseProjection = {};
    };
    
    constexpr uint32 LIGHT_TYPE_DIRECTIONAL = 0;
    constexpr uint32 LIGHT_TYPE_POINT       = 1;
    constexpr uint32 LIGHT_TYPE_SPOT        = 2;

    struct FLight
    {
        glm::vec4 Position      = glm::vec4(0.0f);
        glm::vec4 Direction     = glm::vec4(0.0f);
        glm::vec4 Color         = glm::vec4(0.0f);
        glm::vec2 Angle         = glm::vec2(10.0f);
        float Radius            = 10.0f;
        uint32 Type             = 0;
    };

    struct FSkyLight
    {
        glm::vec4 Color;
    };
    
    struct FSceneLightData
    {
        uint32 NumLights = 0;
        uint32 Padding[3] = {};
        FLight Lights[MAX_LIGHTS];
    };

    struct FSSAOSettings
    {
        float Radius = 1.0f;
        float Intensity = 2.0f;
        float Power = 1.5f;
    };

    struct FEnvironmentSettings
    {
        glm::vec3 SunDirection;
        uint32 Padding0;
    };
    
    struct FGBuffer
    {
        FRHIImageRef Position;
        FRHIImageRef Normals;
        FRHIImageRef Material;
        FRHIImageRef AlbedoSpec;
    };
    
    struct FInstanceData
    {
        glm::mat4 Transform;
    };
    
    struct FSceneRenderStats
    {
        uint32 NumDrawCalls;
        uint64 NumVertices;
        uint64 NumIndices;
    };

    struct FSceneGlobalData
    {
        FCameraData    CameraData;
        float          Time;
        float          DeltaTime;
        float          NearPlane;
        float          FarPlane;
    };

    struct FSceneRenderSettings
    {
        uint8 bHasEnvironment:1 = false;
        uint8 bDrawAABB:1 = false;
        uint8 bSSAO:1 = false;
        FSSAOSettings SSAOSettings;
        FEnvironmentSettings EnvironmentSettings;
    };
}