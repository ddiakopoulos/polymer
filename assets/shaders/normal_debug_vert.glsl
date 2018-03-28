#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

uniform mat4 u_viewProj;
uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;

out vec3 v_normal;

void main()
{
    vec4 worldPos = u_modelMatrix * vec4(inPosition, 1);
    gl_Position = u_viewProj * worldPos;
    v_normal = normalize((u_modelMatrixIT * vec4(inNormal,0)).xyz);
}
