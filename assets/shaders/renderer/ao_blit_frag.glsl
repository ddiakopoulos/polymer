#version 330

uniform sampler2D s_scene;
uniform sampler2D s_ao;
in vec2 v_texcoord0;

out vec4 f_color;

vec3 blend_multiply(vec3 base, vec3 blend) 
{
    return base*blend;
}

vec3 blend_multiply(vec3 base, vec3 blend, float opacity) 
{
    return (blend_multiply(base, blend) * opacity + base * (1.0 - opacity));
}

void main()
{
    vec3 rgb = texture(s_scene, v_texcoord0).rgb;
    float ao = clamp(texture(s_ao, v_texcoord0).r, 0, 1); 
    f_color = vec4(blend_multiply(rgb, vec3(ao), 1), 1.0);
}