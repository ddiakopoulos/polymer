#version 330

uniform sampler2D map;
uniform float useMap;

in vec2 vUV;
in vec4 vColor;
in vec3 vPosition;

out vec4 f_color;

void main()
{
    vec4 c = vColor;
    //if (useMap == 1.) c *= texture(map, vUV);
    f_color = c;
}