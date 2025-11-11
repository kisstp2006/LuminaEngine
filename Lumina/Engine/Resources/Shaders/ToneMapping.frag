#version 450 core
#pragma shader_stage(fragment)

#include "Includes/Common.glsl"

layout(set = 0, binding = 0) uniform sampler2D uHDRSceneColor;

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 oFragColor;


vec3 ACESFilm(vec3 x)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
	vec3 hdrColor = texture(uHDRSceneColor, vTexCoord).rgb;

	vec3 toneMappedColor = ACESFilm(hdrColor);

	toneMappedColor = pow(toneMappedColor, vec3(1.0 / 2.2));

	oFragColor = vec4(toneMappedColor, 1.0);
}