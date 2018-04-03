#version 450

out vec4 out_color;

in vec3 direction;

uniform vec3 A, B, C, D, E, F, G, H, I, Z;
uniform vec3 SunDirection;

// ArHosekSkyModel_GetRadianceInternal
vec3 HosekWilkie(float cos_theta, float gamma, float cos_gamma)
{
    vec3 chi = (1 + cos_gamma * cos_gamma) / pow(1 + H * H - 2 * cos_gamma * H, vec3(1.5));
    return (1 + A * exp(B / (cos_theta + 0.01))) * (C + D * exp(E * gamma) + F * (cos_gamma * cos_gamma) + G * chi + I * sqrt(cos_theta));
}

// clean this up to use shader include

const int MAX_POINT_LIGHTS = 4;
const int NUM_CASCADES = 2;

struct DirectionalLight
{
    vec3 color;
    vec3 direction;
    float amount;
}; 

struct PointLight
{
    vec3 color;
    vec3 position;
    float radius;
};

layout(binding = 0, std140) uniform PerScene
{
    DirectionalLight u_directionalLight;
    PointLight u_pointLights[MAX_POINT_LIGHTS];
    float u_time;
    int u_activePointLights;
    vec2 resolution;
    vec2 invResolution;
    vec4 u_cascadesPlane[NUM_CASCADES];
    mat4 u_cascadesMatrix[NUM_CASCADES];
    float u_cascadesNear[NUM_CASCADES];
    float u_cascadesFar[NUM_CASCADES];
};

// Source: http://alex.vlachos.com/graphics/Alex_Vlachos_Advanced_VR_Rendering_GDC2015.pdf
// Demo: https://www.shadertoy.com/view/MslGR8
vec3 screen_space_dither(vec2 vScreenPos) 
{
    // Iestyn's RGB dither (7 asm instructions) from Portal 2 X360, slightly modified for VR
    vec3 vDither = vec3(dot(vec2(171.0, 231.0), vScreenPos.xy + u_time));
    vDither.rgb = fract(vDither.rgb / vec3(103.0, 71.0, 97.0));
    return vDither.rgb / 255.0;
}

void main()
{
    vec3 V = normalize(direction);
    float cos_theta = clamp(V.y, 0, 1);
    float cos_gamma = clamp(dot(V, SunDirection), 0, 1);
    float gamma_ = acos(cos_gamma);

    vec3 R = Z * HosekWilkie(cos_theta, gamma_, cos_gamma);
    vec3 dither = screen_space_dither(gl_FragCoord.xy);
    out_color = vec4(R + dither, 1);
}