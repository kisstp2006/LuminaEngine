
//// ---------------------------------------------------------------------------------
////  Constants

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;
const float INV_PI = 0.31830988618;


#define LIGHT_INDEX_MASK 0x1FFFu
#define LIGHTS_PER_UINT 2
#define LIGHTS_PER_CLUSTER 100


#define COL_R_SHIFT 0
#define COL_G_SHIFT 8
#define COL_B_SHIFT 16
#define COL_A_SHIFT 24
#define COL_A_MASK 0xFF000000


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
const uint LIGHT_TYPE_DIRECTIONAL = 0;
const uint LIGHT_TYPE_POINT       = 1;
const uint LIGHT_TYPE_SPOT        = 2;


//////////////////////////////////////////////////////////

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

struct FLight
{
    vec3 Position;
    uint Color;

    vec3 Direction;
    float Radius;
    
    vec2 Angles;
    uint Type;
    float Falloff;
};

struct FCluster
{
    vec4 MinPoint;
    vec4 MaxPoint;
    uint Count;
    uint LightIndices[LIGHTS_PER_CLUSTER];
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
    float z = depth * 2.0 - 1.0; // Convert to NDC
    return (2.0 * near * far) / (far + near - z * (far - near));
}

float LinearizeDepthReverseZ(float depth, float near, float far)
{
    return near * far / (far + depth * (near - far));
}