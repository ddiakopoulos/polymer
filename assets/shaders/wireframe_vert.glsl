#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

uniform mat4 u_viewProjMatrix;
uniform vec3 u_eyePos;
uniform mat4 u_modelMatrix = mat4(1.0);

void main()
{
    vec4 worldPosition = u_modelMatrix * vec4(inPosition, 1.0);
    gl_Position = (u_viewProjMatrix * worldPosition);
}
