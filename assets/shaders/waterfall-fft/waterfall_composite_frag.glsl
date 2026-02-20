#version 450

uniform sampler2D s_hdr_color;
uniform sampler2D s_bloom;
uniform float u_bloom_intensity;
uniform float u_exposure;
uniform float u_gamma;
uniform int u_tonemap_mode;  // 0 = none, 1 = Reinhard, 2 = ACES

in vec2 v_texcoord;
out vec4 f_color;

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
    vec3 bloom = texture(s_bloom, v_texcoord).rgb;

    // Combine HDR color with bloom
    vec3 color = hdr_color + bloom * u_bloom_intensity;

    // Apply exposure
    color *= u_exposure;

    // Apply tonemapping
    if (u_tonemap_mode == 2)
        color = aces_film_tonemap(color);
    else if (u_tonemap_mode == 1)
        color = reinhard_tonemap(color);

    // Apply gamma correction
    color = pow(color, vec3(1.0 / u_gamma));

    f_color = vec4(color, 1.0);
}
