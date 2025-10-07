#version 450

#pragma shader_stage(vertex)

#include "Includes/Common.glsl"
#include "Includes/SceneGlobals.glsl"

// Input attributes
layout(location = 0) in vec3 inPosition;      // RGB32_FLOAT
layout(location = 1) in uint inNormal;        // R32_UINT (packed 10:10:10:2)
layout(location = 2) in uvec2 inUV;           // RG16_UINT
layout(location = 3) in vec4 inColor;         // RGBA8_UNORM

layout(push_constant) uniform PushConstants
{
    mat4 uLightSpaceMatrix;
} PC;


void main()
{
    mat4 ModelMatrix = GetModelMatrix(gl_InstanceIndex);
    
    gl_Position = PC.uLightSpaceMatrix * ModelMatrix * vec4(inPosition, 1.0);
}
