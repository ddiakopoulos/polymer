#version 330 core

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 3) in vec2 inTexCoord;

uniform mat4 u_modelMatrix;
uniform mat4 u_lightViewProj;

out vec2 v_texcoord;

void main()
{
    gl_Position = u_lightViewProj * u_modelMatrix * vec4(inPosition, 1.0);
    v_texcoord = inTexCoord;
}
