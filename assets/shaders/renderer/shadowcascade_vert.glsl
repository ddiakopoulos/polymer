#version 450

layout(location = 0) in vec3 inPosition;
uniform mat4 u_modelShadowMatrix;

void main()
{
    gl_Position = u_modelShadowMatrix * vec4(inPosition, 1);
}