#version 330 core

layout (location = 0) in vec3 inPosition;

uniform mat4 u_modelMatrix;
uniform mat4 u_lightViewProj;

out vec3 v_lightWorldPos;

void main()
{
	vec4 worldPosition = u_modelMatrix * vec4(inPosition, 1.0);
    gl_Position = u_lightViewProj * worldPosition;
    v_lightWorldPos = worldPosition.xyz;
}