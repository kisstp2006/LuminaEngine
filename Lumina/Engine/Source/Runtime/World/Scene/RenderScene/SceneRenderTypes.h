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
    class CMaterialInterface;
    struct FVertex;
    class CMaterial;
    class CStaticMesh;
}

namespace Lumina
{

    template<typename T>
    using TRenderVector = TFixedVector<T, 100>;
    
    enum class ERenderSceneDebugFlags : uint8
    {
        None     = 0,
        Position = 1,
        Normals  = 2,
        Albedo   = 3,
        SSAO     = 4,
        Material = 5,
        Depth    = 6,
        Overdraw = 7,
        Cascade1 = 8,
        Cascade2 = 9,
        Cascade3 = 10,
        Cascade4 = 11,
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

    struct FShadowCascade
    {
        FRHIImageRef    ShadowMapImage;
        FLight          DirectionalLight;
        glm::mat4       LightViewProjection;
        float           SplitDepth;
        glm::ivec2      ShadowMapSize;
    };

    struct FSSAOSettings
    {
        float Radius = 1.0f;
        float Intensity = 2.0f;
        float Power = 1.5f;
    };

    struct FEnvironmentSettings
    {
        glm::vec4 SunDirection;
        glm::vec4 AmbientLight; // W is intensity.
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
        glm::mat4   Transform;
        glm::vec4   SphereBounds;
        glm::uvec4  PackedID; // Contains entity ID, and draw ID.
    };
    static_assert(sizeof(FInstanceData) % 16 == 0, "FInstanceData must be 16-byte aligned");

    struct FCullData
    {
        FFrustum    Frustum;
        glm::vec4   View;
    };
    static_assert(sizeof(FCullData) % 16 == 0, "FCullData must be 16-byte aligned");

    
    struct FSceneRenderStats
    {
        uint32 NumDrawCalls;
        uint64 NumVertices;
        uint64 NumIndices;
    };

    struct FSceneGlobalData
    {
        FCameraData     CameraData;
        float           Time;
        float           DeltaTime;
        float           NearPlane;
        float           FarPlane;

        glm::mat4       LightViewProj[4];
        glm::vec4       CascadeSplits;
    };


    struct FSceneRenderSettings
    {
        float CascadeSplitLambda = 0.95;
        uint8 bUseInstancing:1 = true;
        uint8 bHasEnvironment:1 = false;
        uint8 bDrawAABB:1 = false;
        uint8 bSSAO:1 = false;
        FSSAOSettings SSAOSettings;
        FEnvironmentSettings EnvironmentSettings;
    };


    struct FStaticMeshRender
    {
        uint64 SortKey;
        CMaterialInterface* Material;
        CStaticMesh* StaticMesh;
        uint32 FirstIndex;
        uint16 SurfaceIndexCount;

        auto operator <=> (const FStaticMeshRender& Other) const
        {
            return SortKey <=> Other.SortKey;
        }
    };
    
    struct FThreadLocalCollectionData
    {
        TRenderVector<glm::mat4> LocalTransforms;
        TRenderVector<FStaticMeshRender> LocalStaticMeshRenders;
    };

    struct FStaticMeshRenderExtractKey
    {
        typedef uint64 radix_type;
    
        uint64 operator()(const FStaticMeshRender& render) const
        {
            return render.SortKey;
        }
    };

    
}