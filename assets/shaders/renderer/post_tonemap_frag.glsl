#version 330

uniform sampler2D s_texColor;
uniform sampler2D s_bloom;

in vec2 v_texcoord0;

out vec4 f_color;

vec3 aces_film_tonemap(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0, 1);
}

void main()
{
    vec3 rgb = texture(s_texColor, v_texcoord0).rgb;
    rgb += texture(s_bloom, v_texcoord0).rgb; // bright sample
    rgb = aces_film_tonemap(rgb);
    f_color = vec4(rgb, 1.0);
}