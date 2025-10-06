
struct FCameraView
{
    vec4 CameraPosition;    // Camera Position
    mat4 CameraView;        // View matrix
    mat4 InverseCameraView;
    mat4 CameraProjection;  // Projection matrix
    mat4 InverseCameraProjection; // Inverse Camera Projection.
};

const uint LIGHT_TYPE_DIRECTIONAL = 0;
const uint LIGHT_TYPE_POINT       = 1;
const uint LIGHT_TYPE_SPOT        = 2;

struct FInstanceData
{
    mat4    ModelMatrix;
    vec4    SphereBounds;
    uvec4   PackedID;
};

struct FLight
{
    vec4 Position;      // xyz: position, w: range or falloff scale
    vec4 Direction;     // xyz: direction (normalized), w: inner cone cos angle
    vec4 Color;         // rgb: color * intensity, a: unused or padding
    vec2 ConeAngles;    // x: cos(inner cone), y: cos(outer cone)
    float Radius;       // Radius.
    uint Type;          // Type of the light
};

layout(set = 0, binding = 0) readonly uniform SceneGlobals
{
    FCameraView CameraView;
    float Time;
    float DeltaTime;
    float NearPlane;
    float FarPlane;
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

layout(set = 0, binding = 4) uniform FEnvironmentUBO
{
    vec4 SunDirection;
    vec4 AmbientLight;
} EnvironmentUBO;

//layout(set = 0, binding = 5, r32ui) uniform uimage2D uOverdrawImage;

uint DrawIDToInstanceID(uint ID)
{
    return uMappingData.Mapping[ID];
}

vec3 GetSunDirection()
{
    return EnvironmentUBO.SunDirection.xyz;
}

vec3 GetAmbientLightColor()
{
    return EnvironmentUBO.AmbientLight.xyz;
}

float GetAmbientLightIntensity()
{
    return EnvironmentUBO.AmbientLight.w;
}

float GetTime()
{
    return SceneUBO.Time;
}

float GetDeltaTime()
{
    return SceneUBO.DeltaTime;
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

vec3 WorldToView(vec3 worldPos)
{
    return (GetCameraView() * vec4(worldPos, 1.0)).xyz;
}

vec3 NormalWorldToView(vec3 Normal)
{
    return mat3(GetCameraView()) * Normal;
}

vec4 ViewToClip(vec3 viewPos)
{
    return GetCameraProjection() * vec4(viewPos, 1.0);
}

vec4 WorldToClip(vec3 worldPos)
{
    return GetCameraProjection() * GetCameraView() * vec4(worldPos, 1.0);
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