
//// ---------------------------------------------------------------------------------
////  Constants

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;
const float INV_PI = 0.31830988618;

struct DrawIndexedIndirectCommand 
{
    uint IndexCount;
    uint InstanceCount;
    uint FirstIndex;
    int  VertexOffset;
    uint FirstInstance;
};


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

float LinearizeDepth(float Depth, float Near, float Far)
{
    return Near * Far / (Far + Depth * (Near - Far));
}

float LinearizeDepthReverseZ(float depth, float near, float far)
{
    return near * far / (far + depth * (near - far));
}