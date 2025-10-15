#version 450

#pragma shader_stage(vertex)

#include "Includes/SceneGlobals.glsl"

// Input attributes
layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outLightPos;


layout(push_constant) uniform PushConstants
{
    mat4 uLightSpaceMatrix;
    vec3 uLightPos;
    float uLightRadius;
} PC;


void main()
{
    mat4 ModelMatrix = GetModelMatrix(gl_InstanceIndex);
    vec4 WorldPos = ModelMatrix * vec4(inPosition, 1.0);
    
    outWorldPos = WorldPos.xyz;
    outLightPos = PC.uLightPos;
    
    gl_Position = PC.uLightSpaceMatrix * WorldPos;
}
