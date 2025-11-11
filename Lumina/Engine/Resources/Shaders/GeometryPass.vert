#version 460 core

#extension GL_ARB_shader_draw_parameters : enable

#pragma shader_stage(vertex)

#include "Includes/SceneGlobals.glsl"

// Input attributes - updated formats
layout(location = 0) in vec3 inPosition;      // RGB32_FLOAT
layout(location = 1) in uint inNormal;        // R32_UINT (packed 10:10:10:2)
layout(location = 2) in uvec2 inUV;           // RG16_UINT
layout(location = 3) in vec4 inColor;         // RGBA8_UNORM

// Outputs
layout(location = 0) out vec4 outFragColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outNormalWS;
layout(location = 3) out vec4 outFragPos;
layout(location = 4) out vec2 outUV;
layout(location = 5) flat out uint outEntityID;

precise invariant gl_Position;

// Unpack 10:10:10:2 normal
vec3 UnpackNormal(uint packed)
{
    vec3 normal;
    normal.x = float(int(packed << 22) >> 22) / 511.0;
    normal.y = float(int(packed << 12) >> 22) / 511.0;
    normal.z = float(int(packed << 2) >> 22) / 511.0;
    return normalize(normal);
}

// Unpack uint16 UV to float
vec2 UnpackUV(uvec2 packed)
{
    return vec2(packed) / 65535.0;
}

//******* IMPORTANT *******
// Changes to any calculations to gl_Position here, must be exactly reflected in DepthPrePass.vert.

void main()
{
    vec3 normal = UnpackNormal(inNormal);
    vec2 uv = UnpackUV(inUV);

    mat4 ModelMatrix = GetModelMatrix(gl_InstanceIndex);
    mat4 View = GetCameraView();
    mat4 Projection = GetCameraProjection();

    vec4 WorldPos = ModelMatrix * vec4(inPosition, 1.0);
    vec4 ViewPos = View * WorldPos;
    
    // Object-space
    vec3 NormalOS = normal;

    // World-space
    mat3 NormalMatrixWS = transpose(inverse(mat3(ModelMatrix)));
    vec3 NormalWS = NormalMatrixWS * NormalOS;
    
    // View-space
    mat3 NormalMatrixVS = transpose(inverse(mat3(View * ModelMatrix)));
    vec3 NormalVS = NormalMatrixVS * NormalOS;

    // Outputs
    outUV           = vec2(uv.x, uv.y);
    outFragPos      = ViewPos;
    outNormal       = vec4(NormalVS, 1.0);
    outNormalWS     = vec4(NormalWS, 1.0);
    outFragColor    = inColor;
    outEntityID     = GetEntityID(gl_InstanceIndex);
    gl_Position     = Projection * ViewPos;
}
