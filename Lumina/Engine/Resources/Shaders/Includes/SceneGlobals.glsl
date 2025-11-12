//////////////////////////////////////////////////////////

#include "Common.glsl"

layout(set = 0, binding = 0) restrict readonly uniform SceneGlobals
{
    FCameraView CameraView;
    uvec4 ScreenSize;
    uvec4 GridSize;

    float Time;
    float DeltaTime;
    float NearPlane;
    float FarPlane;

    FSSAOSettings SSAOSettings;

} SceneUBO;

layout(set = 0, binding = 1) restrict readonly buffer FModelData
{
    FInstanceData Instances[];
} ModelData;

layout(set = 0, binding = 2) restrict readonly buffer InstanceMappingData
{
    uint Mapping[];
} uMappingData;


layout(set = 0, binding = 3) restrict readonly buffer FLightData
{
    uint    NumLights;
    uint    Padding[3];

    vec3    SunDirection;
    uint    bHasSun;

    vec4    CascadeSplits;

    vec4    AmbientLight;

    FLight  Lights[MAX_LIGHTS];
} LightData;


//////////////////////////////////////////////////////////


uint DrawIDToInstanceID(uint ID)
{
    return uMappingData.Mapping[ID];
}

vec3 GetSunDirection()
{
    return LightData.SunDirection.xyz;
}

vec3 GetAmbientLightColor()
{
    return LightData.AmbientLight.xyz;
}

float GetAmbientLightIntensity()
{
    return LightData.AmbientLight.w;
}

float GetTime()
{
    return SceneUBO.Time;
}

float GetDeltaTime()
{
    return SceneUBO.DeltaTime;
}

float GetNearPlane()
{
    return SceneUBO.NearPlane;
}

float GetFarPlane()
{
    return SceneUBO.FarPlane;
}

vec3 GetCameraPosition()
{
    return SceneUBO.CameraView.CameraPosition.xyx;
}

mat4 GetCameraView()
{
    return SceneUBO.CameraView.CameraView;
}

mat4 GetInverseCameraView()
{
    return SceneUBO.CameraView.InverseCameraView;
}

mat4 GetCameraProjection()
{
    return SceneUBO.CameraView.CameraProjection;
}

mat4 GetInverseCameraProjection()
{
    return SceneUBO.CameraView.InverseCameraProjection;
}

mat4 GetModelMatrix(uint Index)
{
    return ModelData.Instances[DrawIDToInstanceID(Index)].ModelMatrix;
}

vec3 GetModelLocation(uint Index)
{
    mat4 Matrix = GetModelMatrix(Index);
    return vec3(Matrix[3].xyz);
}

uint GetEntityID(uint Index)
{
    return ModelData.Instances[DrawIDToInstanceID(Index)].PackedID.x;
}

vec3 WorldToView(vec3 WorldPos)
{
    return (GetCameraView() * vec4(WorldPos, 1.0)).xyz;
}

vec3 NormalWorldToView(vec3 Normal)
{
    return mat3(GetCameraView()) * Normal;
}

vec4 ViewToClip(vec3 ViewPos)
{
    return GetCameraProjection() * vec4(ViewPos, 1.0);
}

vec4 WorldToClip(vec3 WorldPos)
{
    return GetCameraProjection() * GetCameraView() * vec4(WorldPos, 1.0);
}

float SineWave(float speed, float amplitude)
{
    return amplitude * sin(float(GetTime()) * speed);
}

FLight GetLightAt(uint Index)
{
    return LightData.Lights[Index];
}

uint GetNumLights()
{
    return LightData.NumLights;
}

vec3 ReconstructWorldPos(vec2 uv, float depth, mat4 invProj, mat4 invView)
{
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 view = invProj * clip;
    view /= view.w;
    vec4 world = invView * view;
    return world.xyz;
}

vec3 ReconstructViewPos(vec2 uv, float depth, mat4 invProj)
{
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 view = invProj * clip;
    return view.xyz / view.w;
}