#version 330

layout (location = 0) in vec3 in_position;

uniform mat4 World;
uniform mat4 ViewProjection;

out vec3 direction;

void main()
{
    direction = normalize(in_position);
    vec4 worldPosition = World * vec4(in_position, 1);
	gl_Position = ViewProjection * worldPosition;
}