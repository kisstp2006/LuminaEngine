#version 460 core

#pragma shader_stage(fragment)

#include "Includes/SceneGlobals.glsl"

#define MAX_SCALARS 24
#define MAX_VECTORS 24


layout(early_fragment_tests) in;
layout(location = 0) in vec4 inColor;
layout(location = 1) in vec4 inNormalVS;
layout(location = 2) in vec4 inNormalWS;
layout(location = 3) in vec4 inFragPos;
layout(location = 4) in vec2 inUV;
layout(location = 5) flat in uint inEntityID;

layout(location = 0) out vec4 GNormal;
layout(location = 1) out vec4 GMaterial;
layout(location = 2) out vec4 GAlbedoSpec;
layout(location = 3) out uint GPicker;

layout(set = 1, binding = 0) uniform FMaterialUniforms
{
    vec4 Scalars[MAX_SCALARS / 4];
    vec4 Vectors[MAX_VECTORS];

} MaterialUniforms;

float GetMaterialScalar(uint Index)
{
    uint v = Index / 4;
    uint c = Index % 4;
    return MaterialUniforms.Scalars[v][c];
}

vec4 GetMaterialVec4(uint Index)
{
    return MaterialUniforms.Vectors[Index];
}

vec3 GetWorldNormal(vec3 FragNormal, vec2 UV, vec3 FragPos, vec3 TangentSpaceNormal)
{
    vec3 N = normalize(FragNormal);

    vec3 Q1 = dFdx(FragPos);
    vec3 Q2 = dFdy(FragPos);
    vec2 st1 = dFdx(UV);
    vec2 st2 = dFdy(UV);

    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * TangentSpaceNormal);
}

struct SMaterialInputs
{
    vec3    Diffuse;
    float   Metallic;
    float   Roughness;
    float   Specular;
    vec3    Emissive;
    float   AmbientOcclusion;
    vec3    Normal;
    float   Opacity;
};

uint EntityID       = inEntityID;
vec3 ViewNormal     = normalize(inNormalVS.xyz);
vec3 WorldNormal    = normalize(inNormalWS.xyz);
vec3 WorldPosition  = inFragPos.xyz;
vec2 UV0            = inUV;
vec4 VertexColor    = inColor;


$MATERIAL_INPUTS

void main()
{
    SMaterialInputs Material = GetMaterialInputs();
    
    vec3 EncodedNormal = ViewNormal * 0.5 + 0.5;
    
    GNormal = vec4(EncodedNormal, 1.0);

    GMaterial.r = Material.AmbientOcclusion;
    GMaterial.g = Material.Roughness;
    GMaterial.b = Material.Metallic;
    GMaterial.a = Material.Specular;

    GAlbedoSpec.rgb = Material.Diffuse.rgb;
    GAlbedoSpec.a   = 1.0;

    GPicker = inEntityID;
}
