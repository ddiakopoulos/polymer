#version 330

in vec3 color;
out vec4 f_color;
in vec3 normal;

void main()
{
	f_color = (vec4(color.rgb, 1) * 0.75)+ (dot(normal, vec3(0, 1, 0)) * 0.33);
}