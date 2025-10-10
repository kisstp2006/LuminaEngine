#version 450

#pragma shader_stage(fragment)
layout(early_fragment_tests) in;

#include "Includes/SceneGlobals.glsl"



layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D uPosition;
layout(set = 1, binding = 1) uniform sampler2D uNormal;
layout(set = 1, binding = 2) uniform sampler2D uMaterial;
layout(set = 1, binding = 3) uniform sampler2D uAlbedoSpec;
layout(set = 1, binding = 4) uniform sampler2D uSSAO;
layout(set = 1, binding = 5) uniform sampler2DArray uShadowCascade;

layout(set = 1, binding = 6) readonly buffer ClusterSSBO
{
    FCluster Clusters[];
} Clusters;


float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

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
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// ----------------------------------------------------------------------------

vec3 EvaluateLightContribution(FLight Light, vec3 Position, vec3 N, vec3 V, vec3 Albedo, float Roughness, float Metallic, vec3 F0)
{
    vec3 L;
    float Attenuation = 1.0;
    float Falloff = 1.0;

    if (Light.Type == LIGHT_TYPE_DIRECTIONAL) 
    {
        L = normalize(Light.Direction.xyz);
    }
    else
    {
        vec3 LightToFrag = Light.Position - Position;
        float Distance = length(LightToFrag);
        L = LightToFrag / Distance; 
        Attenuation = 1.0 / (Distance * Distance);

        float DistanceRatio = Distance / Light.Radius;
        float Cutoff = 1.0 - smoothstep(Light.Falloff, 1.0, DistanceRatio);
        Attenuation *= Cutoff;

        if (Light.Type == LIGHT_TYPE_SPOT) 
        {
            vec3 LightDir = normalize(-Light.Direction.xyz);
            float CosTheta = dot(LightDir, L);
            float InnerCos = Light.Angles.x;
            float OuterCos = Light.Angles.y;
            Falloff = clamp((CosTheta - OuterCos) / max(InnerCos - OuterCos, 0.001), 0.0, 1.0);
        }
    }

    vec4 LightColor = GetLightColor(Light);
    LightColor.a *= 100.0f;
    
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

// ----------------------------------------------------------------------------

float TextureProj(vec4 ShadowCoord, int cascade)
{
    float shadow = 0.0;
    float bias = 0.005;

    vec3 projCoords = ShadowCoord.xyz / ShadowCoord.w;

    vec2 shadowUV = projCoords.xy * 0.5 + 0.5;
    //shadowUV.y = 1.0 - shadowUV.y;

    // PCF: Sample 3x3 grid around the fragment
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowCascade, 0).xy);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            vec2 sampleUV = shadowUV + vec2(x, y) * texelSize;
            float shadowDepth = texture(uShadowCascade, vec3(sampleUV, cascade)).r;
            shadow += (projCoords.z - bias > shadowDepth) ? 0.0 : 1.0;
        }
    }

    return shadow / 9.0;
}

void main()
{
    float zNear = GetNearPlane();
    float zFar = GetFarPlane();
    
    uvec3 GridSize = SceneUBO.GridSize.xyz;
    uvec2 ScreenSize = SceneUBO.ScreenSize.xy;
    
    
    vec3 PositionVS = texture(uPosition, vUV).rgb;
    vec3 Position = (GetInverseCameraView() * vec4(PositionVS, 1.0f)).xyz;
    
    vec3 Albedo = texture(uAlbedoSpec, vUV).rgb;
    
    vec3 Normal = texture(uNormal, vUV).rgb * 2.0 - 1.0;
    Normal = normalize(mat3(GetInverseCameraView()) * Normal);
    
    
    float AO = texture(uMaterial, vUV).r;
    float Roughness = texture(uMaterial, vUV).g;
    float Metallic = texture(uMaterial, vUV).b;
    float Specular = texture(uAlbedoSpec, vUV).a;
    float SSAO = texture(uSSAO, vUV).r;

    vec3 N = Normal;
    vec3 V = normalize(SceneUBO.CameraView.CameraPosition.xyz - Position);
    
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, Albedo.rgb, Metallic);
    

    int CascadeIndex = 3;
    float ViewDepth = PositionVS.z;
    float LinearDepth = ViewDepth / SceneUBO.FarPlane;
    LinearDepth = clamp(LinearDepth, 0.0, 1.0);

    for(int i = 0; i < 4 - 1; ++i)
    {
        if(LinearDepth < SceneUBO.CascadeSplits[i])
        {
            CascadeIndex = i;
            break;
        }
    }

    vec4 ShadowCoord = SceneUBO.LightViewProj[CascadeIndex] * vec4(Position, 1.0);
    float Shadow = TextureProj(ShadowCoord, CascadeIndex);
    
    uint zTile = uint((log(abs(PositionVS.z) / zNear) * GridSize.z) / log(zFar / zNear));
    vec2 TileSize = ScreenSize / GridSize.xy;
    uvec3 Tile = uvec3(gl_FragCoord.xy / TileSize, zTile);
    uint TileIndex = Tile.x + (Tile.y * GridSize.x) + (Tile.z * GridSize.x * GridSize.y);
    uint LightCount = Clusters.Clusters[TileIndex].Count;


    vec3 Lo = vec3(0.0);

    if(SceneUBO.bHasSun)
    {
        FLight Light = GetLightAt(0);
        vec3 LContribution = EvaluateLightContribution(Light, Position, N, V, Albedo, Roughness, Metallic, F0);
        LContribution *= Shadow;
        Lo += LContribution;
    }
    
    for (uint i = 0; i < LightCount; ++i)
    {
        uint LightIndex = Clusters.Clusters[TileIndex].LightIndices[i];
        
        FLight Light = GetLightAt(LightIndex);
        vec3 LContribution = EvaluateLightContribution(Light, Position, N, V, Albedo, Roughness, Metallic, F0);
        
        Lo += LContribution;
    }

    vec3 AmbientLightColor = GetAmbientLightColor() * GetAmbientLightIntensity();
    vec3 Ambient = AmbientLightColor * SSAO * AO;
    vec3 Color = Ambient + Lo;

    Color = Color / (Color + vec3(1.0));

    Color = pow(Color, vec3(1.0/2.2));

    outColor = vec4(Color, 1.0);

    vec3 CascadeDebugColors[4] = vec3[4](
    vec3(1.0, 0.0, 0.0), // Cascade 0 = red
    vec3(0.0, 1.0, 0.0), // Cascade 1 = green
    vec3(0.0, 0.0, 1.0), // Cascade 2 = blue
    vec3(1.0, 1.0, 0.0)  // Cascade 3 = yellow
    );
    
    //outColor *= vec4(CascadeDebugColors[CascadeIndex], 1.0);

    return;
}
