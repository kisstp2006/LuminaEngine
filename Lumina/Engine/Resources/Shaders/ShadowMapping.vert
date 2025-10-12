#version 450

#pragma shader_stage(vertex)

#include "Includes/SceneGlobals.glsl"

// Input attributes
layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform PushConstants
{
    mat4 uLightSpaceMatrix;
} PC;


void main()
{
    mat4 ModelMatrix = GetModelMatrix(gl_InstanceIndex);
    
    gl_Position = PC.uLightSpaceMatrix * ModelMatrix * vec4(inPosition, 1.0);
}
