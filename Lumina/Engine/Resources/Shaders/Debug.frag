#version 450 core
#pragma shader_stage(fragment)

#include "Includes/Common.glsl"
#include "Includes/SceneGlobals.glsl"

layout(set = 1, binding = 0) uniform sampler2D uRenderTarget;
layout(set = 1, binding = 1) uniform sampler2D uPositionDepth;
layout(set = 1, binding = 2) uniform sampler2D uNormal;
layout(set = 1, binding = 3) uniform sampler2D uAlbedo;
layout(set = 1, binding = 4) uniform sampler2D uSSAO;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 OutFragColor;

#define DEBUG_NONE     0
#define DEBUG_POSITION 1
#define DEBUG_NORMALS  2
#define DEBUG_ALBEDO   3
#define DEBUG_SSAO     4
#define DEBUG_MATERIAL 5
#define DEBUG_DEPTH    6
#define DEBUG_OVERDRAW 7

layout(push_constant) uniform DebugInfo
{
    uint DebugFlags;
} Debug;

void main()
{
    if (Debug.DebugFlags == DEBUG_NONE)
    {
        OutFragColor = texture(uRenderTarget, inUV);
        return;
    }

    if (Debug.DebugFlags == DEBUG_POSITION)
    {
        vec3 pos = texture(uPositionDepth, inUV).xyz;
        OutFragColor = vec4(pos, 1.0);
        return;
    }
    
    if (Debug.DebugFlags == DEBUG_NORMALS)
    {
        vec3 Normal = texture(uNormal, inUV).rgb * 2.0 - 1.0;
        Normal = normalize(mat3(GetInverseCameraView()) * Normal);
        OutFragColor = vec4(Normal, 1.0);
        return;
    }
    
    if (Debug.DebugFlags == DEBUG_ALBEDO)
    {
        OutFragColor = texture(uAlbedo, inUV);
        return;
    }
    
    if (Debug.DebugFlags == DEBUG_SSAO)
    {
        float ssao = texture(uSSAO, inUV).r;
        OutFragColor = vec4(vec3(ssao), 1.0);
        return;
    }
    
    if (Debug.DebugFlags == DEBUG_MATERIAL)
    {
        vec3 material = texture(uAlbedo, inUV).rgb;
        OutFragColor = vec4(material, 1.0);
        return;
    }
    
    if (Debug.DebugFlags == DEBUG_DEPTH)
    {
        float depth = texture(uPositionDepth, inUV).a;
        OutFragColor = vec4(vec3(depth), 1.0);
        return;
    }

    //if (Debug.DebugFlags == DEBUG_OVERDRAW)
    //{
    //    ivec2 size = imageSize(uOverdrawImage);
    //    ivec2 pix = ivec2(inUV * vec2(size));
    //    uint count = imageLoad(uOverdrawImage, pix).r;
    //
    //    uint maxExpected = 8u;
    //    float t = clamp(float(count) / float(maxExpected), 0.0, 1.0);
    //
    //    vec3 color = mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), t);
    //    OutFragColor = vec4(color, 1.0);
    //    return;
    //}
}
