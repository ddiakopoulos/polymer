#version 330

in vec3 v_normal;
out vec4 f_color;

void main()
{
    f_color = vec4(v_normal, 1.0);
}
