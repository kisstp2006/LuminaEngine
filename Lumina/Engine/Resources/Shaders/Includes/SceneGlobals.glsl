//////////////////////////////////////////////////////////

#include "Common.glsl"

layout(set = 0, binding = 0) readonly uniform SceneGlobals
{
    FCameraView CameraView;
    uvec4 ScreenSize;
    uvec4 GridSize;

    float Time;
    float DeltaTime;
    float NearPlane;
    float FarPlane;


    mat4    LightViewProj[4];
    vec4    CascadeSplits;

    vec3    SunDirection;
    bool    bHasSun;

    vec4    AmbientLight;

    FSSAOSettings SSAOSettings;

} SceneUBO;

layout(set = 0, binding = 1) readonly buffer FModelData
{
    FInstanceData Instances[];
} ModelData;

layout(set = 0, binding = 2) readonly buffer InstanceMappingData
{
    uint Mapping[];
} uMappingData;


layout(set = 0, binding = 3) readonly buffer FLightData
{
    uint    NumLights;
    FLight  Lights[];
} LightData;


//////////////////////////////////////////////////////////


uint DrawIDToInstanceID(uint ID)
{
    return uMappingData.Mapping[ID];
}

vec3 GetSunDirection()
{
    return SceneUBO.SunDirection.xyz;
}

vec3 GetAmbientLightColor()
{
    return SceneUBO.AmbientLight.xyz;
}

float GetAmbientLightIntensity()
{
    return SceneUBO.AmbientLight.w;
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