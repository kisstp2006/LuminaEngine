#version 450 core

#pragma shader_stage(fragment)

#include "Includes/SceneGlobals.glsl"

layout(set = 1, binding = 0) uniform sampler2D uDepth;
layout(set = 1, binding = 1) uniform sampler2D uNormal;
layout(set = 1, binding = 2) uniform sampler2D uNoise;


layout(location = 0) in vec2 inUV;

layout(location = 0) out float outFragColor;

void main()
{
    float Depth = texture(uDepth, inUV).r;

    // FragPos and Normal are in *view-space*.
    vec3 FragPos = ReconstructViewPos(inUV, Depth, GetInverseCameraProjection());
    vec3 Normal = normalize(texture(uNormal, inUV).rgb) * 2.0 - 1.0;
    
    ivec2 TexDim = textureSize(uDepth, 0);
    ivec2 NoiseDim = textureSize(uNoise, 0);
    
    const vec2 NoiseUV = vec2(float(TexDim.x) / float(NoiseDim.x), float(TexDim.y) / float(NoiseDim.y)) * inUV;
    vec3 RandomVec = texture(uNoise, NoiseUV).xyz * 2.0 - 1.0;
    
    vec3 Tangent = normalize(RandomVec - Normal * dot(RandomVec, Normal));
    vec3 BiTangent = cross(Tangent, Normal);
    mat3 TBN = mat3(Tangent, BiTangent, Normal);
    
    float Occlusion = 0.0f;
    const float Bias = 0.025f;
    
    for(int i = 0; i < SSAO_KERNEL_SIZE; i++)
    {
        // Get sample position.
        vec3 SamplerPos = TBN * SceneUBO.SSAOSettings.Samples[i].xyz; // Tangent-Space to View-Space
        SamplerPos = FragPos + SamplerPos * SceneUBO.SSAOSettings.Radius;
        
        // Get sample position in screen space.
        vec4 Offset = vec4(SamplerPos, 1.0f);
        Offset = GetCameraProjection() * Offset; // From View-Space to Clip-Space.
        Offset.xy /= Offset.w; // Perspective divide.
        Offset.xy = Offset.xy * 0.5f + 0.5f; // Transform to range 0.0 - 1.0

        float SampleDepth = texture(uDepth, Offset.xy).r;
        vec3 SamplePos = ReconstructViewPos(Offset.xy, SampleDepth, GetInverseCameraProjection());

        
        float RangeCheck = smoothstep(0.0f, 1.0f, SceneUBO.SSAOSettings.Radius / abs(FragPos.z - SamplePos.z));
        
        // Now both are in view-space, so the comparison is valid
        Occlusion += (SamplePos.z >= SamplerPos.z + Bias ? 0.0f : 1.0f) * RangeCheck;
    }
    
    float Power = SceneUBO.SSAOSettings.Power;
    Occlusion = 1.0 - (Occlusion / float(SSAO_KERNEL_SIZE));
    outFragColor = pow(Occlusion, Power);
}
