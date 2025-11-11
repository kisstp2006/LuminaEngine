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

layout(location = 0) out vec4 outColor;
layout(location = 1) out uint GPicker;

layout(set = 1, binding = 0) uniform sampler2DArray uShadowCascade;
layout(set = 1, binding = 1) uniform samplerCubeArray uShadowCubemaps;
layout(set = 1, binding = 2) uniform sampler2D uShadowAtlas;
layout(set = 1, binding = 3) uniform sampler2D uSSAO;

// Cluster data
layout(set = 1, binding = 4) readonly buffer ClusterSSBO
{
    FCluster Clusters[];
} Clusters;

// Material uniforms
layout(set = 2, binding = 0) uniform FMaterialUniforms
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


uint EntityID       = inEntityID;
vec3 ViewNormal     = normalize(inNormalVS.xyz);
vec3 WorldNormal    = normalize(inNormalWS.xyz);
vec3 ViewPosition   = inFragPos.xyz;
vec3 WorldPosition  = (GetInverseCameraView() * vec4(inFragPos.xyz, 1.0)).xyz;
vec2 UV0            = inUV;
vec4 VertexColor    = inColor;

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
    vec3    WorldPositionOffset;
};

$MATERIAL_INPUTS

// ============================================================================
// PBR FUNCTIONS
// ============================================================================

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a         = roughness * roughness;
    float a2        = a * a;
    float NdotH     = max(dot(N, H), 0.0);
    float NdotH2    = NdotH * NdotH;

    float nom       = a2;
    float denom     = (NdotH2 * (a2 - 1.0) + 1.0);
    denom           = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r     = (roughness + 1.0);
    float k     = (r * r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
// ----------------------------------------------------------------------------


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

// ============================================================================
// SHADOW FUNCTIONS
// ============================================================================

float Shadow_Directional(float bias, vec3 worldPos, vec3 viewPos)
{
    int CascadeIndex = 3;
    
    for(int i = 0; i < 3; ++i)
    {
        if(viewPos.z < LightData.CascadeSplits[i])
        {
            CascadeIndex = i;
            break;
        }
    }
    
    FLight Light = GetLightAt(0);
    
    vec4 shadowCoord = Light.ViewProjection[CascadeIndex] * vec4(worldPos, 1.0);
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    vec2 uv = projCoords.xy * 0.5 + 0.5;
    float currentDepth = projCoords.z;
    
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowCascade, 0));
    float blockerDepthSum = 0.0;
    int blockerCount = 0;
    
    float searchRadius = 2.0;
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            vec2 offsetUV = uv + vec2(x, y) * texelSize * searchRadius;
            offsetUV = clamp(offsetUV, vec2(0.0), vec2(1.0));
            
            float shadowMapDepth = texture(uShadowCascade, vec3(offsetUV, CascadeIndex)).r;
            if (shadowMapDepth < currentDepth - bias)
            {
                blockerDepthSum += shadowMapDepth;
                blockerCount++;
            }
        }
    }
    
    float penumbraSize = 0.0;
    if (blockerCount > 0)
    {
        float avgBlockerDepth = blockerDepthSum / float(blockerCount);
        float lightSize = 0.5;
        penumbraSize = (currentDepth - avgBlockerDepth) * lightSize / avgBlockerDepth;
        penumbraSize = clamp(penumbraSize, 0.0, 3.0);
    }
    
    float shadow = 0.0;
    int sampleCount = 0;
    
    float kernelSize = mix(0.5, 3.0, penumbraSize);
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            vec2 offsetUV = uv + vec2(x, y) * texelSize * kernelSize;
            offsetUV = clamp(offsetUV, vec2(0.0), vec2(1.0));
            
            float shadowMapDepth = texture(uShadowCascade, vec3(offsetUV, CascadeIndex)).r;
            shadow += (currentDepth - bias > shadowMapDepth) ? 0.0 : 1.0;
            sampleCount++;
        }
    }
    
    return shadow / float(sampleCount);
}

float Shadow_Point(FLight light, vec3 worldPos, float bias)
{
    vec3 L = worldPos - light.Position;
    float distanceToLight = length(L);
    float currentDepth = distanceToLight / light.Radius;
    vec3 dir = normalize(L);
    dir.y = -dir.y;
    
    vec3 up = abs(dir.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
    vec3 right = normalize(cross(up, dir));
    up = normalize(cross(dir, right));
    
    const float SearchOffsetAngle = 0.01;
    float blockerDepthSum = 0.0;
    int blockerCount = 0;
    
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            vec3 sampleDir = normalize(dir + right * SearchOffsetAngle * x + up * SearchOffsetAngle * y);
            float shadowDepth = texture(uShadowCubemaps, vec4(sampleDir, light.Shadow.ShadowMapIndex)).r;
            
            if (shadowDepth < currentDepth - bias)
            {
                blockerDepthSum += shadowDepth;
                blockerCount++;
            }
        }
    }
    
    float penumbraSize = 0.0;
    if (blockerCount > 0)
    {
        float avgBlockerDepth = blockerDepthSum / float(blockerCount);
        float lightSize = 0.3;
        penumbraSize = (currentDepth - avgBlockerDepth) * lightSize / avgBlockerDepth;
        penumbraSize = clamp(penumbraSize, 0.0, 2.0);
    }
    
    float shadowOffset = mix(0.003, 0.015, penumbraSize);
    float shadow = 0.0;
    int sampleCount = 0;
    
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            vec3 sampleDir = normalize(dir + right * shadowOffset * x + up * shadowOffset * y);
            float shadowDepth = texture(uShadowCubemaps, vec4(sampleDir, light.Shadow.ShadowMapIndex)).r;
            shadow += (currentDepth - bias > shadowDepth) ? 0.0 : 1.0;
            sampleCount++;
        }
    }
    
    return shadow / float(sampleCount);
}

float Shadow_Spot(FLight light, vec3 worldPos, float bias)
{
    vec4 shadowCoord = light.ViewProjection[0] * vec4(worldPos, 1.0);
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    
    if(projCoords.z > 1.0)
    {
        return 0.0;
    }
    
    vec3 L = worldPos - light.Position;
    float distanceToLight = length(L);
    float currentDepth = distanceToLight / light.Radius;
    
    vec2 localUV = projCoords.xy * 0.5 + 0.5;
    vec2 atlasUV = light.Shadow.AtlasUVOffset + (localUV * light.Shadow.AtlasUVScale);
    
    vec2 atlasTexelSize = 1.0 / vec2(textureSize(uShadowAtlas, 0));
    vec2 tileTexelSize = atlasTexelSize / light.Shadow.AtlasUVScale;
    
    float blockerDepthSum = 0.0;
    int blockerCount = 0;
    
    float searchRadius = 2.0;
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            vec2 offsetLocalUV = localUV + vec2(x, y) * tileTexelSize * searchRadius;
            offsetLocalUV = clamp(offsetLocalUV, vec2(0.0), vec2(1.0));
            
            vec2 offsetAtlasUV = light.Shadow.AtlasUVOffset + (offsetLocalUV * light.Shadow.AtlasUVScale);
            
            float shadowMapDepth = texture(uShadowAtlas, offsetAtlasUV).r;
            if (shadowMapDepth < currentDepth - bias)
            {
                blockerDepthSum += shadowMapDepth;
                blockerCount++;
            }
        }
    }
    
    float penumbraSize = 0.0;
    if (blockerCount > 0)
    {
        float avgBlockerDepth = blockerDepthSum / float(blockerCount);
        float lightSize = 0.5;
        penumbraSize = (currentDepth - avgBlockerDepth) * lightSize / avgBlockerDepth;
        penumbraSize = clamp(penumbraSize, 0.0, 3.0);
    }
    
    float shadow = 0.0;
    int sampleCount = 0;
    
    float kernelSize = mix(0.5, 3.0, penumbraSize);
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            vec2 offsetLocalUV = localUV + vec2(x, y) * tileTexelSize * kernelSize;
            offsetLocalUV = clamp(offsetLocalUV, vec2(0.0), vec2(1.0));
            
            vec2 offsetAtlasUV = light.Shadow.AtlasUVOffset + (offsetLocalUV * light.Shadow.AtlasUVScale);
            
            float shadowMapDepth = texture(uShadowAtlas, offsetAtlasUV).r;
            shadow += (currentDepth - bias > shadowMapDepth) ? 0.0 : 1.0;
            sampleCount++;
        }
    }
    
    return shadow / float(sampleCount);
}

float ComputeShadow(FLight light, vec3 worldPos, vec3 viewPos, float bias)
{
    if (HasFlag(light.Flags, LIGHT_FLAG_TYPE_DIRECTIONAL))
    {
        return Shadow_Directional(0.00005, worldPos, viewPos);
    }
    else if (HasFlag(light.Flags, LIGHT_FLAG_TYPE_POINT))
    {
        return Shadow_Point(light, worldPos, bias);
    }
    else if (HasFlag(light.Flags, LIGHT_FLAG_TYPE_SPOT))
    {
        return Shadow_Spot(light, worldPos, bias);
    }
    else
    {
        return 1.0;
    }
}

float ComputeShadowBias(FLight Light, vec3 Normal, vec3 LightDir, float CascadeIndex)
{
    float ConstBias = 0.000025;
    float SlopeBias = 0.00001;
    
    float DiffuseFactor = dot(Normal, -LightDir);
    DiffuseFactor = clamp(DiffuseFactor, 0.0, 1.0);
    
    float Bias = mix(ConstBias, SlopeBias, DiffuseFactor);
    
    if (HasFlag(Light.Flags, LIGHT_FLAG_TYPE_DIRECTIONAL))
    {
        Bias *= mix(1.0, 2.0, CascadeIndex / 3.0);
    }
    
    return 0.0;
}

// ============================================================================
// LIGHTING
// ============================================================================

vec3 EvaluateLightContribution(FLight Light, vec3 Position, vec3 N, vec3 V, vec3 Albedo, float Roughness, float Metallic, vec3 F0)
{
    vec3 L;
    float Attenuation = 1.0;
    float Falloff = 1.0;
    vec4 LightColor = GetLightColor(Light);

    if (HasFlag(Light.Flags, LIGHT_FLAG_TYPE_DIRECTIONAL))
    {
        L = normalize(Light.Direction.xyz);
    }
    else
    {
        LightColor.a *= 100.0f;
        vec3 LightToFrag = Light.Position - Position;
        float Distance = length(LightToFrag);
        L = LightToFrag / Distance;
        Attenuation = 1.0 / (Distance * Distance);

        float DistanceRatio = Distance / Light.Radius;
        float Cutoff = 1.0 - smoothstep(Light.Falloff, 1.0, DistanceRatio);
        Attenuation *= Cutoff;

        if (HasFlag(Light.Flags, LIGHT_FLAG_TYPE_SPOT))
        {
            vec3 LightDir = normalize(Light.Direction.xyz);
            float CosTheta = dot(LightDir, L);
            float InnerCos = Light.Angles.x;
            float OuterCos = Light.Angles.y;
            Falloff = clamp((CosTheta - OuterCos) / max(InnerCos - OuterCos, 0.001), 0.0, 1.0);
        }
    }


    // Radiance
    vec3 Radiance = LightColor.rgb * LightColor.a * Attenuation * Falloff;

    // Half vector
    vec3 H = normalize(V + L);

    // BRDF terms
    float NDF = DistributionGGX(N, H, Roughness);
    float G   = GeometrySmith(N, V, L, Roughness);
    vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 Numerator   = NDF * G * F;
    float Denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 Spec = Numerator / Denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - Metallic);

    float NdotL = max(dot(N, L), 0.0);

    return (kD * Albedo / PI + Spec) * Radiance * NdotL;
}

// ============================================================================
// MAIN
// ============================================================================

void main()
{
    // Get material properties
    SMaterialInputs Material = GetMaterialInputs();
    
    // Early discard for fully transparent fragments
    if (Material.Opacity <= 0.0)
    {
        discard;
    }
    
    vec3 Albedo     = Material.Diffuse;
    float Alpha     = Material.Opacity;
    float AO        = Material.AmbientOcclusion;
    float Roughness = Material.Roughness;
    float Metallic  = Material.Metallic;
    float Specular  = Material.Specular;
    //float SSAO      = Mate;

    vec3 Position = (GetInverseCameraView() * vec4(ViewPosition, 1.0f)).xyz;

    // Setup geometry
    vec3 TSN        = normalize(Material.Normal);
    vec3 N          = GetWorldNormal(WorldNormal, inUV, WorldPosition, TSN);
    vec3 V          = normalize(SceneUBO.CameraView.CameraPosition.xyz - WorldPosition);
    
    // Calculate Fresnel reflectance at normal incidence
    vec3 IOR    = vec3(0.5);
    vec3 F0     = abs((1.0 - IOR) / (1.0 + IOR));
    F0          = F0 * F0;
    F0          = mix(F0, Albedo, Metallic);
    
    // Calculate view space position for clustering
    mat4 ViewMatrix = GetCameraView();

    // Cluster calculation
    float zNear         = GetNearPlane();
    float zFar          = GetFarPlane();
    uvec3 GridSize      = SceneUBO.GridSize.xyz;
    uvec2 ScreenSize    = SceneUBO.ScreenSize.xy;
    
    uint zTile      = uint((log(abs(ViewPosition.z) / zNear) * GridSize.z) / log(zFar / zNear));
    vec2 TileSize   = vec2(ScreenSize) / vec2(GridSize.xy);
    uvec3 Tile      = uvec3(gl_FragCoord.xy / TileSize, zTile);
    uint TileIndex  = Tile.x + (Tile.y * GridSize.x) + (Tile.z * GridSize.x * GridSize.y);
    uint LightCount = Clusters.Clusters[TileIndex].Count;
    
    // Lighting accumulation
    vec3 Lo = vec3(0.0);
    
    // Directional light (sun)
    if(LightData.bHasSun)
    {
        FLight Light = GetLightAt(0);
        float Shadow = ComputeShadow(Light, WorldPosition, ViewPosition, 0.0005);
        vec3 LContribution = EvaluateLightContribution(Light, WorldPosition, N, V, Albedo, Roughness, Metallic, F0);
        Lo += LContribution * Shadow;
    }
    
    // Clustered lights
    for (uint i = 0; i < LightCount; ++i)
    {
        uint LightIndex = Clusters.Clusters[TileIndex].LightIndices[i];
        FLight Light = GetLightAt(LightIndex);
        
        float Shadow = 1.0;
        if(Light.Shadow.ShadowMapIndex != INDEX_NONE)
        {
            vec3 L = WorldPosition - Light.Position;
            vec3 DirectionToLight = normalize(L);
            float DistanceToLight = length(L);
            float AttenuationFactor = 1.0 / (DistanceToLight * DistanceToLight);
            float BiasScale = mix(0.1, 1.0, AttenuationFactor);
            
            float Bias = ComputeShadowBias(Light, N, DirectionToLight, 0) * BiasScale;
            Shadow = ComputeShadow(Light, WorldPosition, ViewPosition, Bias);
        }
        
        vec3 LContribution = EvaluateLightContribution(Light, WorldPosition, N, V, Albedo, Roughness, Metallic, F0);
        Lo += LContribution * Shadow;
    }
    
    // Add ambient lighting
    vec3 AmbientLightColor = GetAmbientLightColor() * GetAmbientLightIntensity();
    vec3 Ambient = AmbientLightColor * AO;
    vec3 Color = Ambient + Lo;
    
    // Add emissive
    Color += Material.Emissive;
    
    // Output
    outColor = vec4(Color, Material.Opacity);
    GPicker = inEntityID;
}