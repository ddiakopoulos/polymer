#version 330

uniform mat4 u_mvp;

layout(location = 0) in vec3 inPosition;
out vec3 position;

void main()
{
    gl_Position = u_mvp * vec4(inPosition, 1.0);
   	position = inPosition;
}