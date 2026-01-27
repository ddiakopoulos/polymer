#version 330

uniform sampler2D s_texColor;
uniform float u_exposure = 1.0;
uniform float u_gamma = 2.2;
uniform int u_tonemapMode = 2;  // 0 = none, 1 = Reinhard, 2 = ACES

in vec2 v_texcoord0;

out vec4 f_color;

vec3 uncharted2_tonemap(in vec3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 apply_uncharted2_tonemap(in vec3 color)
{
    float bias = 2.0;
    vec3 curr = uncharted2_tonemap(bias * color);
    vec3 white_scale = 1.0 / uncharted2_tonemap(vec3(11.2));
    return curr * white_scale;
}

vec3 aces_film_tonemap(in vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 reinhard_tonemap(in vec3 color)
{
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float mapped_luminance = luminance / (1.0 + luminance);
    return (mapped_luminance / max(luminance, 0.0001)) * color;
}

void main()
{
    vec3 color = texture(s_texColor, v_texcoord0).rgb * u_exposure;

    if (u_tonemapMode == 2)
        color = aces_film_tonemap(color);
    else if (u_tonemapMode == 1)
        color = reinhard_tonemap(color);

    f_color = vec4(pow(color, vec3(1.0 / u_gamma)), 1.0);
}