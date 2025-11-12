#version 460
#extension GL_ARB_shader_viewport_layer_array : require
#extension GL_EXT_multiview : require

#pragma shader_stage(vertex)
#include "Includes/SceneGlobals.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outLightPos;

layout(push_constant) uniform PushConstants
{
    uint Mask;
} PC;

void main()
{
    uint LightNum = PC.Mask >> 16;
    uint FaceIndex = PC.Mask & 0xFFFF;
    
    FLight Light = LightData.Lights[LightNum];
    mat4 ModelMatrix = GetModelMatrix(gl_InstanceIndex);
    vec4 WorldPos = ModelMatrix * vec4(inPosition, 1.0);
    
    outWorldPos = WorldPos.xyz;
    outLightPos = Light.Position;
    
    gl_Position = Light.ViewProjection[FaceIndex] * WorldPos;
}