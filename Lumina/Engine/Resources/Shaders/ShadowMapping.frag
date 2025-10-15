#version 450

#pragma shader_stage(fragment)

#include "Includes/Common.glsl"

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inLightPos;

layout(push_constant) uniform PushConstants
{
    mat4 uLightSpaceMatrix;
    vec3 uLightPos;
    float uLightRadius;
} PC;

void main()
{
    float Distance = length(inWorldPos - inLightPos);
    
    // Normalize to 0-1 and give linear depth.
    gl_FragDepth = Distance / PC.uLightRadius;
}
