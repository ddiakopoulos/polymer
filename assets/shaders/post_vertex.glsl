#version 330

layout(location = 0) in vec3 position;

// Output data - will be interpolated for each fragment

out vec2 texCoord;

void main()
{
	gl_Position = vec4(position, 1);
    texCoord = (position.xy + vec2(1,1)) / 2.0;
}