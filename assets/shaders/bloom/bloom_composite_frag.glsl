#version 450

uniform sampler2D s_hdr_color;
uniform sampler2D s_bloom_0;
uniform sampler2D s_bloom_1;
uniform sampler2D s_bloom_2;
uniform sampler2D s_bloom_3;
uniform sampler2D s_bloom_4;
uniform float u_bloom_strength;
uniform float u_bloom_radius;
uniform float u_exposure;
uniform float u_gamma;
uniform int u_tonemap_mode;

in vec2 v_texcoord;
out vec4 f_color;

float lerp_bloom_factor(float factor)
{
    return mix(factor, 1.2 - factor, u_bloom_radius);
}

vec3 reinhard_tonemap(vec3 color)
{
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float mapped_luminance = luminance / (1.0 + luminance);
    return (mapped_luminance / max(luminance, 0.0001)) * color;
}

vec3 aces_film_tonemap(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 hdr_color = texture(s_hdr_color, v_texcoord).rgb;

    vec3 bloom = vec3(0.0);
    bloom += lerp_bloom_factor(1.0) * texture(s_bloom_0, v_texcoord).rgb;
    bloom += lerp_bloom_factor(0.8) * texture(s_bloom_1, v_texcoord).rgb;
    bloom += lerp_bloom_factor(0.6) * texture(s_bloom_2, v_texcoord).rgb;
    bloom += lerp_bloom_factor(0.4) * texture(s_bloom_3, v_texcoord).rgb;
    bloom += lerp_bloom_factor(0.2) * texture(s_bloom_4, v_texcoord).rgb;

    vec3 color = hdr_color + bloom * u_bloom_strength;

    color *= u_exposure;

    if (u_tonemap_mode == 2)
        color = aces_film_tonemap(color);
    else if (u_tonemap_mode == 1)
        color = reinhard_tonemap(color);

    color = pow(color, vec3(1.0 / u_gamma));

    f_color = vec4(color, 1.0);
}
