#version 330

uniform sampler2D s_texColor;

in vec2 v_texcoord0;

out vec4 f_color;

const float gamma     = 2.2;
const float exposure  = 1.0;
const float pureWhite = 1.0;

vec3 uncharted2_tonemap(in vec3 x) 
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;
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
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main()
{
    vec3 color = texture(s_texColor, v_texcoord0).rgb * exposure;
    f_color = vec4(color, 1); //vec4(apply_uncharted2_tonemap(color), 1.0);

    // Reinhard tonemapping operator - see: "Photographic Tone Reproduction for Digital Images", eq. 4
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float mappedLuminance = (luminance * (1.0 + luminance/(pureWhite*pureWhite))) / (1.0 + luminance);

    // Scale color by ratio of average luminances
    vec3 mappedColor = (mappedLuminance / luminance) * color;

    // Output with gamma correction
    //f_color = vec4(pow(mappedColor, vec3(1.0/gamma)), 1.0);
    //f_color = vec4(vec3(luminance), 1.0);
}