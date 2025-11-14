
//// ---------------------------------------------------------------------------------
////  Constants

const float PHI     = 1.618033988749895; // Golden ratio
const float PI      = 3.1415926535897932384626433832795;
const float TWO_PI  = 6.28318530718;
const float INV_PI  = 0.31830988618;


#define LIGHT_INDEX_MASK 0x1FFFu
#define LIGHTS_PER_UINT 2
#define LIGHTS_PER_CLUSTER 100
#define SHADOW_SAMPLE_COUNT 32

#define INDEX_NONE -1

#define COL_R_SHIFT 0
#define COL_G_SHIFT 8
#define COL_B_SHIFT 16
#define COL_A_SHIFT 24
#define COL_A_MASK 0xFF000000

#define MAX_LIGHTS 1728
#define MAX_SHADOWS 100
#define NUM_CASCADES 4


struct FDrawIndexedIndirectArguments 
{
    uint IndexCount;
    uint InstanceCount;
    uint FirstIndex;
    int  VertexOffset;
    uint FirstInstance;
};

struct FCameraView
{
    vec4 CameraPosition;    // Camera Position
    mat4 CameraView;        // View matrix
    mat4 InverseCameraView;
    mat4 CameraProjection;  // Projection matrix
    mat4 InverseCameraProjection; // Inverse Camera Projection.
};

//////////////////////////////////////////////////////////

const int SSAO_KERNEL_SIZE        = 32;

//////////////////////////////////////////////////////////


const uint LIGHT_FLAG_TYPE_DIRECTIONAL = 1 << 0;
const uint LIGHT_FLAG_TYPE_POINT       = 1 << 1;
const uint LIGHT_FLAG_TYPE_SPOT        = 1 << 2;
const uint LIGHT_FLAG_CASTSHADOW       = 1 << 3;


//////////////////////////////////////////////////////////

vec2 VogelDiskSample(int SampleIndex, int SamplesCount, float Angle)
{
    float GoldenAngle   = 2.0 * PI * (1.0 - 1.0 / PHI);
    float r             = sqrt(float(SampleIndex) + 0.5) / sqrt(float(SamplesCount));
    float theta         = float(SampleIndex) * GoldenAngle + Angle;
    
    return vec2(r * cos(theta), r * sin(theta));
}

struct FSSAOSettings
{
    float Radius;
    float Intensity;
    float Power;

    vec4 Samples[SSAO_KERNEL_SIZE];
};

struct FInstanceData
{
    mat4    ModelMatrix;
    vec4    SphereBounds;
    uvec4   PackedID;
};

struct FLightShadow
{
    vec2 AtlasUVOffset;
    vec2 AtlasUVScale;
    int ShadowMapIndex; // -1 means no shadow.
    uint Pad[3];
};

struct FLight
{
    vec3 Position;
    uint Color;

    float Intensity;
    //... 12 bytes of padding.
    
    vec3 Direction;
    float Radius;
    
    mat4 ViewProjection[6];
    
    vec2 Angles;
    uint Flags;
    float Falloff;

    FLightShadow Shadow[6];
};

struct FCluster
{
    vec4 MinPoint;
    vec4 MaxPoint;
    uint LightIndices[LIGHTS_PER_CLUSTER];
    uint Count;
};


struct FCullData
{
    vec4 Planes[6];
    vec4 View;
};

vec4 UnpackColor(uint ColorMask)
{
    return vec4
    (
        float((ColorMask >> COL_R_SHIFT) & 0xFF) / 255.0,
        float((ColorMask >> COL_G_SHIFT) & 0xFF) / 255.0,
        float((ColorMask >> COL_B_SHIFT) & 0xFF) / 255.0,
        float((ColorMask >> COL_A_SHIFT) & 0xFF) / 255.0
    );
}

vec4 GetLightColor(FLight Light)
{
    return UnpackColor(Light.Color);
}

// Check if *any* of the bits in 'flag' are set in 'value'
bool HasFlag(uint value, uint flag)
{
    return (value & flag) != 0u;
}

// Check if *all* bits in 'flag' are set in 'value'
bool HasAllFlags(uint value, uint flag)
{
    return (value & flag) == flag;
}

// Set bits in 'flag' on 'value'
uint SetFlag(uint value, uint flag)
{
    return value | flag;
}

// Clear bits in 'flag' from 'value'
uint ClearFlag(uint value, uint flag)
{
    return value & (~flag);
}

// Toggle bits in 'flag' on 'value'
uint ToggleFlag(uint value, uint flag)
{
    return value ^ flag;
}



vec3 ReconstructWorldPosition(vec2 uv, float depth, mat4 invProjection, mat4 invView) 
{
    float z = depth * 2.0 - 1.0;
    
    vec4 ClipSpacePos = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 ViewSpacePos = invProjection * ClipSpacePos;
    
    ViewSpacePos /= ViewSpacePos.w;
    
    vec4 WorldSpacePosition = invView * ViewSpacePos;
    
    return WorldSpacePosition.xyz;
}

vec3 ReconstructViewPosition(vec2 uv, float depth, mat4 invProjection)
{
    // Convert UV and reversed depth to clip space
    vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);

    // Transform to view space
    vec4 viewSpace = invProjection * clipSpace;

    // Perspective divide
    return viewSpace.xyz / viewSpace.w;
}

float LinearizeDepth(float depth, float near, float far) 
{
    return (near * far) / (far - depth * (far - near)) / far;
}

float LinearizeDepthReverseZ(float depth, float near, float far)
{
    return near * far / (far + depth * (near - far));
}