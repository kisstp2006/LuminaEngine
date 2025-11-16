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
layout(set = 1, binding = 1) uniform sampler2D uShadowAtlas;

// Cluster data
layout(set = 1, binding = 2) restrict readonly buffer ClusterSSBO
{
    FCluster Clusters[];
} Clusters;

// Material uniforms
layout(set = 2, binding = 0) uniform FMaterialUniforms
{
    vec4 Scalars[MAX_SCALARS / 4];
    vec4 Vectors[MAX_VECTORS];
} MaterialUniforms;





#define PCF_SAMPLES_DIV_2 2




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

struct FCubemapCoord
{
    int Face;      // 0-5 (±X, ±Y, ±Z)
    vec2 UV;       // [0,1] range within face
};

FCubemapCoord DirectionToCubemapCoord(vec3 dir)
{
    FCubemapCoord Result;

    vec3 absDir = abs(dir);

    float isXMax = step(absDir.y, absDir.x) * step(absDir.z, absDir.x);
    float isYMax = step(absDir.x, absDir.y) * step(absDir.z, absDir.y);
    float isZMax = 1.0 - isXMax - isYMax;

    vec3 faceIndex = vec3(
    mix(1.0, 0.0, step(0.0, dir.x)), // X: +X=0, -X=1
    mix(3.0, 2.0, step(0.0, dir.y)), // Y: +Y=2, -Y=3
    mix(5.0, 4.0, step(0.0, dir.z))  // Z: +Z=4, -Z=5
    );

    Result.Face = int(isXMax * faceIndex.x + isYMax * faceIndex.y + isZMax * faceIndex.z);
    bool bDownFace = Result.Face == 3;

    float signX = sign(dir.x);
    float signY = sign(dir.y);
    float signZ = sign(dir.z);

    vec2 uvX = vec2(-signX * dir.z, dir.y) / absDir.x;
    vec2 uvY = vec2(dir.x, signY * dir.z) / absDir.y;
    vec2 uvZ = vec2(signZ * dir.x, dir.y) / absDir.z;

    Result.UV = isXMax * uvX + isYMax * uvY + isZMax * uvZ;

    Result.UV.y = mix(Result.UV.y, -Result.UV.y, float(bDownFace));

    Result.UV = Result.UV * 0.5 + 0.5;

    return Result;
}

float ComputeShadowFactor(FLight Light, vec3 FragmentPos, float Bias)
{
    bool IsPointLight       = Light.Flags == LIGHT_FLAG_TYPE_POINT;
    bool IsSpotLight        = Light.Flags == LIGHT_FLAG_TYPE_SPOT;
    bool IsDirectionalLight = Light.Flags == LIGHT_FLAG_TYPE_DIRECTIONAL;

    int ViewProjectionIndex = 0;
    if(IsDirectionalLight)
    {
        for (int i = 0; i < 3; ++i)
        {
            ViewProjectionIndex = max(ViewProjectionIndex, int(step(LightData.CascadeSplits[i], ViewPosition.z)) * i);
        }
    }
    
    vec4 ShadowCoord        = Light.ViewProjection[ViewProjectionIndex] * vec4(FragmentPos, 1.0);
    vec3 ProjCoords         = ShadowCoord.xyz / max(ShadowCoord.w, 1.0);
    vec2 ProjectionUV       = ProjCoords.xy * 0.5 + 0.5;

    vec3 L                  = FragmentPos - Light.Position;
    float DistanceToLight   = length(L);
    float CurrentDepth      = DistanceToLight / max(Light.Radius, 1.0);
    vec3 Dir                = normalize(L);

    FCubemapCoord Coord     = DirectionToCubemapCoord(Dir);
    Coord.UV                = mix(Coord.UV, ProjectionUV, float(IsSpotLight));
    Coord.Face              = int(mix(float(Coord.Face), 0.0, float(IsSpotLight)));
    
    FLightShadow ShadowTile = Light.Shadow[Coord.Face];
    float Angle             = fract(sin(dot(FragmentPos.xy, vec2(12.9898, 78.233))) * 43758.5453) * 6.28318530718;
    
    float Shadow = 0.0f;
    for(int i = 0; i < SHADOW_SAMPLE_COUNT; i++)
    {
        float MinRadius     = 0.00001;
        float MaxRadius     = 0.0015;
        float Exponent      = 1.5;
        
        float FilterRadius  = mix(MinRadius, MaxRadius, pow(CurrentDepth, Exponent));
        float ShadowDepth   = 0.0f;
        vec2 Offset         = VogelDiskSample(i, SHADOW_SAMPLE_COUNT, Angle) * FilterRadius;
        
        if(IsDirectionalLight)
        {
            vec2 CascadeUV  = ProjectionUV + Offset;
            ShadowDepth     = texture(uShadowCascade, vec3(CascadeUV, ViewProjectionIndex)).r;
            Shadow          += step(ShadowDepth, CurrentDepth - Bias);
        }
        else
        {
            // We apply a clamp due to a small pixel difference in the atlas.
            vec2 ClampedUV  = clamp(Coord.UV + (Offset / ShadowTile.AtlasUVScale), 0.001, 0.999);
            vec2 SampleUV   = ShadowTile.AtlasUVOffset + (ClampedUV * ShadowTile.AtlasUVScale);
            
            ShadowDepth     = texture(uShadowAtlas, SampleUV).r;
            Shadow          += step(CurrentDepth - Bias, ShadowDepth);
        }
    }
    
    return Shadow / float(SHADOW_SAMPLE_COUNT);
}

float ComputeShadowBias(FLight Light, vec3 Normal, vec3 LightDir, float CascadeIndex)
{
    float ConstBias = 0.000025;
    float SlopeBias = 0.00001;
    
    float DiffuseFactor = dot(Normal, LightDir);
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
    bool bSpotLight = Light.Flags == LIGHT_FLAG_TYPE_SPOT;
    bool bDirectional = Light.Flags == LIGHT_FLAG_TYPE_DIRECTIONAL;
    
    vec3 L;
    float Attenuation   = 1.0;
    float Falloff       = 1.0;
    vec4 LightColor     = GetLightColor(Light);

    if (bDirectional)
    {
        L = normalize(Light.Direction.xyz);
    }
    else
    {
        vec3 LightToFrag    = Light.Position - Position;
        float Distance      = length(LightToFrag);
        L                   = LightToFrag / Distance;
        Attenuation         = 1.0 / (Distance * Distance);

        float DistanceRatio = Distance / Light.Radius;
        float Cutoff        = 1.0 - smoothstep(Light.Falloff, 1.0, DistanceRatio);
        Attenuation         *= Cutoff;

        if (bSpotLight)
        {
            vec3 LightDir   = normalize(Light.Direction.xyz);
            float CosTheta  = dot(LightDir, L);
            float InnerCos  = Light.Angles.x;
            float OuterCos  = Light.Angles.y;
            Falloff         = clamp((CosTheta - OuterCos) / max(InnerCos - OuterCos, 0.001), 0.0, 1.0);
        }
    }


    // Radiance
    vec3 Radiance = LightColor.rgb * Light.Intensity * Attenuation * Falloff;

    // Half vector
    vec3 H = normalize(V + L);

    // BRDF terms
    float NDF = DistributionGGX(N, H, Roughness);
    float G   = GeometrySmith(N, V, L, Roughness);
    vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 Numerator      = NDF * G * F;
    float Denominator   = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 Spec           = Numerator / Denominator;

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
    SMaterialInputs Material = GetMaterialInputs();

    vec3 Albedo     = Material.Diffuse;
    float Alpha     = Material.Opacity;
    float AO        = Material.AmbientOcclusion;
    float Roughness = Material.Roughness;
    float Metallic  = Material.Metallic;
    float Specular  = Material.Specular;

    vec3 Position   = (GetInverseCameraView() * vec4(ViewPosition, 1.0f)).xyz;

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
    
    vec3 Lo = vec3(0.0);
    
    if(LightData.bHasSun != 0)
    {
        FLight Light        = GetLightAt(0);
        float Shadow        = ComputeShadowFactor(Light, WorldPosition, 0.0005);
        vec3 LContribution  = EvaluateLightContribution(Light, WorldPosition, N, V, Albedo, Roughness, Metallic, F0);
        Lo += LContribution * Shadow;
    }
    
    for (uint i = 0; i < LightCount; ++i)
    {
        uint LightIndex = Clusters.Clusters[TileIndex].LightIndices[i];
        FLight Light    = GetLightAt(LightIndex);
        
        float Shadow = 1.0;
        if(Light.Shadow[0].ShadowMapIndex != INDEX_NONE)
        {
            vec3 L                  = WorldPosition - Light.Position;
            vec3 DirectionToLight   = normalize(L);
            float DistanceToLight   = length(L);
            float AttenuationFactor = 1.0 / (DistanceToLight * DistanceToLight);
            float BiasScale         = mix(0.1, 1.0, AttenuationFactor);
            
            float Bias = ComputeShadowBias(Light, N, DirectionToLight, 0) * BiasScale;
            Shadow = ComputeShadowFactor(Light, WorldPosition, Bias);
        }
        
        vec3 LContribution = EvaluateLightContribution(Light, WorldPosition, N, V, Albedo, Roughness, Metallic, F0);
        Lo += LContribution * Shadow;
    }
    
    vec3 AmbientLightColor  = GetAmbientLightColor() * GetAmbientLightIntensity();
    vec3 Ambient            = AmbientLightColor * AO;
    vec3 Color              = Ambient + Lo;
    
    Color                   += Material.Emissive;

    outColor    = vec4(Color, Material.Opacity);
    GPicker     = inEntityID;
}