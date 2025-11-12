#version 450

#pragma shader_stage(fragment)

#include "Includes/SceneGlobals.glsl"

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inLightPos;

layout(push_constant) uniform PushConstants
{
    uint Mask;
} PC;

void main()
{
    float Distance = length(inWorldPos - inLightPos);

    uint LightNum = PC.Mask >> 16;
    uint FaceIndex = PC.Mask & 0xFFFF;
    
    FLight Light = GetLightAt(LightNum);

    // Normalize to 0-1 and give linear depth.
    gl_FragDepth = Distance / Light.Radius;
}
