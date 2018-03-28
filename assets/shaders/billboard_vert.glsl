#version 330

uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;
uniform mat4 u_viewProj;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 3) in vec2 inTexcoord;

out vec3 v_position, v_normal;
out vec2 v_texcoord;
out vec4 v_clipspace;

void main()
{
    v_position = (u_modelMatrix * vec4(inPosition, 1)).xyz;
    v_normal = normalize((u_modelMatrixIT * vec4(inNormal,0)).xyz);
    v_texcoord = vec2(inTexcoord.x, inTexcoord.y);
    gl_Position = u_viewProj * vec4(v_position,1);
    v_clipspace = u_viewProj * vec4(v_position,1);
}