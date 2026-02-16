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

// Uncharted 2 / Filmic (Hable)
const float FILMIC_A = 0.15;
const float FILMIC_B = 0.50;
const float FILMIC_C = 0.10;
const float FILMIC_D = 0.20;
const float FILMIC_E = 0.02;
const float FILMIC_F = 0.30;
const float FILMIC_W = 11.2;

vec3 uncharted2Tonemap(vec3 x)
{
    return ((x * (FILMIC_A * x + FILMIC_C * FILMIC_B) + FILMIC_D * FILMIC_E) /
            (x * (FILMIC_A * x + FILMIC_B) + FILMIC_D * FILMIC_F)) - FILMIC_E / FILMIC_F;
}

vec3 tonemap_filmic(vec3 color)
{
    color = uncharted2Tonemap(color * u_exposure);
    vec3 whiteScale = 1.0 / uncharted2Tonemap(vec3(FILMIC_W));
    return color * whiteScale;
}

// Hejl-Burgess-Dawson
vec3 tonemap_hejl(vec3 color)
{
    color *= u_exposure;
    const float A = 0.22, B = 0.3, C = 0.1, D = 0.2, E = 0.01, F = 0.3;
    const float Scl = 1.25;
    vec3 h = max(vec3(0.0), color - vec3(0.004));
    return (h * ((Scl * A) * h + Scl * vec3(C * B)) + Scl * vec3(D * E)) /
           (h * (A * h + vec3(B)) + vec3(D * F)) - Scl * vec3(E / F);
}

// ACES 2.0 (Stephen Hill approximation)
const mat3 ACESInputMat = mat3(
    0.59719, 0.35458, 0.04823,
    0.07600, 0.90834, 0.01566,
    0.02840, 0.13383, 0.83777
);

const mat3 ACESOutputMat = mat3(
     1.60475, -0.53108, -0.07367,
    -0.10208,  1.10813, -0.00605,
    -0.00327, -0.07276,  1.07602
);

vec3 RRTAndODTFit(vec3 v)
{
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 tonemap_aces_2(vec3 color)
{
    color *= u_exposure / 0.6;
    color = color * ACESInputMat;
    color = RRTAndODTFit(color);
    color = color * ACESOutputMat;
    return clamp(color, 0.0, 1.0);
}

// ACES 1.0 (Narkowicz)
vec3 tonemap_aces_1(vec3 color)
{
    float tA = 2.51;
    float tB = 0.03;
    float tC = 2.43;
    float tD = 0.59;
    float tE = 0.14;
    vec3 x = color * u_exposure;
    return clamp((x * (tA * x + tB)) / (x * (tC * x + tD) + tE), 0.0, 1.0);
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

    if (u_tonemap_mode == 4)
        color = tonemap_aces_1(color);
    else if (u_tonemap_mode == 3)
        color = tonemap_aces_2(color);
    else if (u_tonemap_mode == 2)
        color = tonemap_hejl(color);
    else if (u_tonemap_mode == 1)
        color = tonemap_filmic(color);
    else
        color *= u_exposure;

    color = pow(color, vec3(1.0 / u_gamma));

    f_color = vec4(color, 1.0);
}
