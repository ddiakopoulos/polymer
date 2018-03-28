#version 330

uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;
uniform mat4 u_viewProj;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 3) in vec2 inTexcoord;

out vec3 v_position;
out vec3 v_normal;
out vec2 v_texcoord;

void main()
{
    vec4 worldPos = u_modelMatrix * vec4(inPosition, 1);
    gl_Position = u_viewProj * worldPos;
    v_position = worldPos.xyz;
    v_normal = normalize((u_modelMatrixIT * vec4(inNormal,0)).xyz);
    v_texcoord = inTexcoord;
}