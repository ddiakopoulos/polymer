#version 330

out vec4 f_color;

uniform samplerCube sc_ibl;

in vec3 position;

void main()
{ 
    f_color = texture(sc_ibl, position); 
}