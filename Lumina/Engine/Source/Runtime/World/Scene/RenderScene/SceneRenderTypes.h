#pragma once

#include <glm/glm.hpp>

#include "Core/Math/Transform.h"
#include "Platform/GenericPlatform.h"
#include "Renderer/MeshData.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderResource.h"
#include "Renderer/RHIFwd.h"
#include "Renderer/RHIGlobals.h"

#define MAX_LIGHTS 1728
#define MAX_SHADOWS 100
#define SSAO_KERNEL_SIZE 32
#define LIGHT_INDEX_MASK 0x1FFFu
#define LIGHTS_PER_UINT 2
#define LIGHTS_PER_CLUSTER 100

#define SCENE_MAX_BOUNDS UINT64_MAX

#define COL_R_SHIFT 0
#define COL_G_SHIFT 8
#define COL_B_SHIFT 16
#define COL_A_SHIFT 24
#define COL_A_MASK 0xFF000000

#define VERIFY_SSBO_ALIGNMENT(Type) \
static_assert(sizeof(Type) % 16 == 0, #Type " must be 16-byte aligned");

constexpr unsigned int ClusterGridSizeX = 16;
constexpr unsigned int ClusterGridSizeY = 9;
constexpr unsigned int ClusterGridSizeZ = 24;

constexpr unsigned int NumClusters = ClusterGridSizeX * ClusterGridSizeY * ClusterGridSizeZ;

constexpr int GShadowMapResolution      = 4096;
constexpr int GShadowAtlasResolution    = 8192;
constexpr int GShadowCubemapResolution  = 512;
constexpr int GMaxPointLightShadows     = 100;

namespace Lumina
{
    class FDeferredRenderScene;
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
        None                = 0,
        Position            = 1,
        Normals             = 2,
        Albedo              = 3,
        SSAO                = 4,
		AmbientOcclusion    = 5,
		Roughness           = 6,
		Metallic            = 7,
		Specular            = 8,
        Depth               = 9,
        ShadowAtlas		    = 10,
    };
    
    struct FCameraData
    {
        glm::vec4 Location          = {};
        glm::mat4 View              = {};
        glm::mat4 InverseView       = {};
        glm::mat4 Projection        = {};
        glm::mat4 InverseProjection = {};
    };

    constexpr uint32 LIGHT_TYPE_MASK      = 0x0000FFFF; // lower 16 bits
    constexpr uint32 LIGHT_SHADOW_MASK    = 0xFFFF0000; // upper 16 bits
    constexpr int    LIGHT_SHADOW_SHIFT   = 16;
    
    constexpr uint32 LIGHT_TYPE_DIRECTIONAL = 1 << 0;
    constexpr uint32 LIGHT_TYPE_POINT       = 1 << 1;
    constexpr uint32 LIGHT_TYPE_SPOT        = 1 << 2;

    struct FShadowAtlasConfig
    {
        uint32 AtlasResolution = 8192;
        uint32 TileResolution = 512;

        constexpr uint32 TilesPerRow() const { return AtlasResolution / TileResolution; }
        constexpr uint32 MaxTiles() const { return TilesPerRow() * TilesPerRow(); }
    };

    struct FShadowTile
    {
        glm::vec2 UVOffset;     // Normalized offset (0-1 range)
        glm::vec2 UVScale;      // Normalized scale (1.0 / TilesPerRow)
        bool bInUse = false;
    };
    
    class FShadowAtlas
    {
    public:
        
        FShadowAtlas(const FShadowAtlasConfig& InConfig)
            : Config(InConfig)
        {
            FRHIImageDesc ImageDesc;
            ImageDesc.Extent = glm::uvec2(InConfig.AtlasResolution);
            ImageDesc.Flags.SetMultipleFlags(EImageCreateFlags::DepthAttachment, EImageCreateFlags::ShaderResource);
            ImageDesc.Format = EFormat::D32;
            ImageDesc.bKeepInitialState = true;
            ImageDesc.InitialState = EResourceStates::ShaderResource;
            ImageDesc.Dimension = EImageDimension::Texture2D;
            ImageDesc.DebugName = "Shadow Atlas";

            ShadowAtlas = GRenderContext->CreateImage(ImageDesc);
            
            Tiles.resize(Config.MaxTiles());
            float Scale = 1.0f / Config.TilesPerRow();
        
            for (uint32 y = 0; y < Config.TilesPerRow(); ++y)
            {
                for (uint32 x = 0; x < Config.TilesPerRow(); ++x)
                {
                    uint32 Index = y * Config.TilesPerRow() + x;
                    Tiles[Index].UVOffset = glm::vec2(x * Scale, y * Scale);
                    Tiles[Index].UVScale = glm::vec2(Scale, Scale);
                }
            }
        }
    
        int32 AllocateTile()
        {
            for (uint32 i = 0; i < Tiles.size(); ++i)
            {
                if (!Tiles[i].bInUse)
                {
                    Tiles[i].bInUse = true;
                    return static_cast<int32>(i);
                }
            }
            return INDEX_NONE; // Atlas full
        }
    
        void FreeTile(int32 TileIndex)
        {
            if (TileIndex >= 0 && TileIndex < Tiles.size())
            {
                Tiles[TileIndex].bInUse = false;
            }
        }

        void FreeTiles()
        {
            for (FShadowTile& Tile : Tiles)
            {
                Tile.bInUse = false;
            }
        }
        
        const FShadowTile& GetTile(int32 TileIndex) const { return Tiles[TileIndex]; }
        FRHIImageRef GetImage() const { return ShadowAtlas; }

    private:

        FRHIImageRef ShadowAtlas;
        FShadowAtlasConfig Config;
        TVector<FShadowTile> Tiles;
    };
    

    struct FLightShadow
    {
        glm::vec2   AtlasUVOffset;
        glm::vec2   AtlasUVScale;
        
        int32       ShadowMapIndex;
        uint32      Padding0[3];
    };
    
    VERIFY_SSBO_ALIGNMENT(FLightShadow)
    
    struct alignas(16) FLight
    {
        glm::vec3       Position;
        uint32          Color;
        
        float           Intensity;
        uint32          Padding0[3];
        
        glm::vec3       Direction;
        float           Radius;

        glm::mat4       ViewProjection[6];
        
        glm::vec2       Angles;
        uint32          Flags;
        float           Falloff;

        FLightShadow    Shadow;
    };

    static_assert(eastl::is_trivially_copyable_v<FLight>);

    VERIFY_SSBO_ALIGNMENT(FLight)
    
    
    struct FSkyLight
    {
        glm::vec4 Color;
    };
    
    struct FSceneLightData
    {
        uint32          NumLights = 0;
        uint32          Padding0[3] = {};

        glm::vec3       SunDirection;
        bool            bHasSun;

        glm::vec4       CascadeSplits;

        glm::vec4       AmbientLight;
        
        FLight          Lights[MAX_LIGHTS];
    };

    struct FShadowCascade
    {
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

        uint32 Padding;

        glm::vec4 Samples[SSAO_KERNEL_SIZE];
    };

    struct FGBuffer
    {
        FRHIImageRef Normals;
        FRHIImageRef Material;
        FRHIImageRef AlbedoSpec;
    };

    struct alignas(16) FCluster
    {
        glm::vec4 MinPoint;
        glm::vec4 MaxPoint;
        uint32 LightIndices[LIGHTS_PER_CLUSTER];
        uint32 Count;
    };
    
    VERIFY_SSBO_ALIGNMENT(FCluster)
    
    struct FLightClusterPC
    {
        glm::mat4 InverseProjection;
        glm::vec2 zNearFar;
        glm::uvec2 ScreenSize;
        glm::uvec4 GridSize;
    };

    struct FInstanceData
    {
        glm::mat4   Transform;
        glm::vec4   SphereBounds;
        glm::uvec4  PackedID;
    };
    
    VERIFY_SSBO_ALIGNMENT(FInstanceData)

    
    struct FCullData
    {
        FFrustum    Frustum;
        glm::vec4   View;
    };
    
    VERIFY_SSBO_ALIGNMENT(FCullData)
    

    struct FSceneGlobalData
    {
        FCameraData     CameraData;
        glm::uvec4      ScreenSize;
        glm::uvec4      GridSize;

        float           Time;
        float           DeltaTime;
        float           NearPlane;
        float           FarPlane;
        
        FSSAOSettings   SSAOSettings;
    };

    struct FMeshPass
    {
        uint32 MeshDrawOffset;
        uint32 MeshDrawSize;
        uint32 IndirectDrawOffset;
    };


    struct FSceneRenderSettings
    {
        float CascadeSplitLambda = 0.95;
        uint8 bUseInstancing:1 = true;
        uint8 bHasEnvironment:1 = false;
        uint8 bDrawAABB:1 = false;
        uint8 bSSAO:1 = false;
    };
}