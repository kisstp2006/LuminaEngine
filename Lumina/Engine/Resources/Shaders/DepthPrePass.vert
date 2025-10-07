#version 460 core
#extension GL_ARB_shader_draw_parameters : enable

#pragma shader_stage(vertex)

#include "Includes/Common.glsl"
#include "Includes/SceneGlobals.glsl"

// Input attributes
layout(location = 0) in vec3 inPosition;

precise invariant gl_Position;

//******* IMPORTANT *******
// Changes to any calculations to gl_Position here, must be exactly reflected in Material.vert.

void main()
{
    mat4 ModelMatrix = GetModelMatrix(gl_InstanceIndex);
    mat4 View = GetCameraView();
    mat4 Projection = GetCameraProjection();

    vec4 WorldPos = ModelMatrix * vec4(inPosition, 1.0);
    vec4 ViewPos = View * WorldPos;
    
    gl_Position = Projection * ViewPos;
}
