#version 330

uniform sampler2D s_texColor;
uniform sampler2D s_bloom;

in vec2 v_texcoord0;

out vec4 f_color;

const float gamma     = 2.2;
const float exposure  = 1.0;
const float pureWhite = 1.0;

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
    color += texture(s_bloom, v_texcoord0).rgb; // bright sample
    //f_color = vec4(aces_film_tonemap(color), 1.0);

    // Reinhard tonemapping operator - see: "Photographic Tone Reproduction for Digital Images", eq. 4
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float mappedLuminance = (luminance * (1.0 + luminance/(pureWhite*pureWhite))) / (1.0 + luminance);

    // Scale color by ratio of average luminances
    vec3 mappedColor = (mappedLuminance / luminance) * color;

    // Output with gamma correction
    //f_color = vec4(pow(mappedColor, vec3(1.0/gamma)), 1.0);
    f_color = vec4(mappedColor, 1.0);
}