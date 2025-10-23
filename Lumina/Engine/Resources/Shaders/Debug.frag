#version 450 core
#pragma shader_stage(fragment)

#include "Includes/SceneGlobals.glsl"

layout(set = 1, binding = 0) uniform sampler2D uRenderTarget;
layout(set = 1, binding = 1) uniform sampler2D uDepth;
layout(set = 1, binding = 2) uniform sampler2D uNormal;
layout(set = 1, binding = 3) uniform sampler2D uAlbedo;
layout(set = 1, binding = 4) uniform sampler2D uSSAO;
layout(set = 1, binding = 5) uniform sampler2D uMaterial;
layout(set = 1, binding = 6) uniform sampler2D uShadowAtlas;


layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 OutFragColor;


#define DEBUG_NONE              0
#define DEBUG_POSITION          1
#define DEBUG_NORMALS           2
#define DEBUG_ALBEDO            3
#define DEBUG_SSAO              4
#define DEBUG_AMBIENT_OCCLUSION 5
#define DEBUG_ROUGHNESS         6
#define DEBUG_METALLIC          7
#define DEBUG_SPECULAR          8
#define DEBUG_DEPTH             9
#define DEBUG_SHADOW_ATLAS      10

layout(push_constant) uniform DebugInfo
{
    uint DebugFlags;
} Debug;

void main()
{
    if (Debug.DebugFlags == DEBUG_NONE)
    {
        OutFragColor = texture(uRenderTarget, vUV);
        return;
    }

    if (Debug.DebugFlags == DEBUG_POSITION)
    {
        float Depth = texture(uDepth, vUV).r;
        vec3 PositionVS = ReconstructViewPos(vUV, Depth, GetInverseCameraProjection());
        OutFragColor = vec4(PositionVS, 1.0);
        return;
    }
    
    if (Debug.DebugFlags == DEBUG_NORMALS)
    {
        vec3 Normal = texture(uNormal, vUV).rgb * 2.0 - 1.0;
        Normal = normalize(mat3(GetInverseCameraView()) * Normal);
        OutFragColor = vec4(Normal, 1.0);
        return;
    }
    
    if (Debug.DebugFlags == DEBUG_ALBEDO)
    {
        OutFragColor = texture(uAlbedo, vUV);
        return;
    }
    
    if (Debug.DebugFlags == DEBUG_SSAO)
    {
        float ssao = texture(uSSAO, vUV).r;
        OutFragColor = vec4(vec3(ssao), 1.0);
        return;
    }

    if(Debug.DebugFlags == DEBUG_AMBIENT_OCCLUSION)
    {
        float ao = texture(uMaterial, vUV).r;
        OutFragColor = vec4(vec3(ao), 1.0);
        return;
    }

    if(Debug.DebugFlags == DEBUG_ROUGHNESS)
    {
        float Roughness = texture(uMaterial, vUV).g;
        OutFragColor = vec4(vec3(Roughness), 1.0);
        return;
    }

    if(Debug.DebugFlags == DEBUG_METALLIC)
    {
        float Metallic = texture(uMaterial, vUV).b;
        OutFragColor = vec4(vec3(Metallic), 1.0);
        return;
    }

    if(Debug.DebugFlags == DEBUG_SPECULAR)
    {
        float Specular = texture(uMaterial, vUV).a;
        OutFragColor = vec4(vec3(Specular), 1.0);
        return;
    }
    

    if (Debug.DebugFlags == DEBUG_DEPTH)
    {
        float depth = texture(uDepth, vUV).r;
        float LinearDepth = LinearizeDepth(depth, GetFarPlane(), GetNearPlane());
        float VisualizedDepth = LinearDepth / GetFarPlane();

        OutFragColor = vec4(vec3(VisualizedDepth), 1.0);

        return;
    }

    if(Debug.DebugFlags == DEBUG_SHADOW_ATLAS)
    {
        float Shadow = texture(uShadowAtlas, vUV).r;
        Shadow *= 1.5;
        OutFragColor = vec4(vec3(Shadow), 1.0);
        return;
    }

    OutFragColor.a = 1.0;
}
