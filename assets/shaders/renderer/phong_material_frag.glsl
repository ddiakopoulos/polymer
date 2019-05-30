#include "renderer_common.glsl"
#include "colorspace_conversions.glsl"

in vec3 v_world_position;
in vec3 v_view_space_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_tangent;
in vec3 v_bitangent;

// Material Uniforms
uniform vec3 u_diffuseColor;
uniform vec3 u_specularColor;
uniform float u_specularShininess;
uniform float u_specularStrength;

uniform sampler2D s_diffuse;
uniform sampler2D s_normal;

out vec4 f_color;

vec3 lambert_diffuse(const in vec3 diffuseColor) 
{
    return INV_PI * diffuseColor;
} 

// Optimized variant (presented by Epic at SIGGRAPH '13)
// https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
vec3 schlick_approx(const in vec3 specularColor, const in float LdotH)
{
    const float fresnel = exp2((-5.55473 * LdotH - 6.98316) * LdotH);
    return (1.0 - specularColor) * fresnel + specularColor;
}

vec3 blinn_phong_specular(const in float NdotH, const in float LdotH, const in vec3 specularColor, const in float shininess) 
{
    const vec3 F = schlick_approx(specularColor, LdotH);
    const float D = INV_PI * (shininess * 0.5 + 1.0) * pow(NdotH, shininess);
    return F * (0.25 * D); // implicit geometry term
}

void main()
{   
    vec3 diffuseColor = u_diffuseColor;
    vec3 N = normalize(v_normal);

#ifdef HAS_NORMAL_MAP
    vec3 nSample = normalize(texture(s_normal, v_texcoord).xyz * 2.0 - 1.0);
    N = normalize(calc_normal_map(v_normal, normalize(v_tangent), normalize(v_bitangent), nSample).xyz);
#endif

#ifdef HAS_DIFFUSE_MAP
    diffuseColor *= sRGBToLinear(texture(s_diffuse, v_texcoord).rgb, DEFAULT_GAMMA); 
#endif

    // View direction
    vec3 V = normalize(u_eyePos.xyz - v_world_position);
    float NdotV = abs(dot(N, V)) + 0.001;

    vec3 Lo = vec3(0, 0, 0);

    // Compute directional light
    if (sunlightActive > 0)
    {
        vec3 L = normalize(u_directionalLight.direction); // incident light direction
        vec3 H = normalize(L + V);  
        float NdotL = clamp(dot(N, L), 0.001, 1.0);
        float NdotH = clamp(dot(N, H), 0.0, 1.0);
        float LdotH = clamp(dot(L, H), 0.0, 1.0);

        const vec3 irradiance = NdotL * u_directionalLight.color;

        vec3 diffuseContrib, specContrib;
        diffuseContrib += irradiance * lambert_diffuse(diffuseColor);
        specContrib += irradiance * blinn_phong_specular(NdotH, LdotH, u_specularColor, u_specularShininess) * u_specularStrength;

        Lo += diffuseContrib + specContrib;
    }
    
    // Compute point lights
    for (int i = 0; i < u_activePointLights; ++i)
    {
        vec3 L = normalize(u_pointLights[i].position - v_world_position); 
        vec3 H = normalize(L + V);  

        float NdotL = clamp(dot(N, L), 0.001, 1.0);
        float NdotH = clamp(dot(N, H), 0.0, 1.0);
        float LdotH = clamp(dot(L, H), 0.0, 1.0);

        float dist = length(u_pointLights[i].position - v_world_position);
        float attenuation = point_light_attenuation(u_pointLights[i].radius, 2.0, 0.1, dist); // reasonable intensity is 0.01 to 8

        const vec3 irradiance = NdotL * u_pointLights[i].color;

        vec3 diffuseContrib, specContrib;
        diffuseContrib += irradiance * lambert_diffuse(diffuseColor);
        specContrib += irradiance * blinn_phong_specular(NdotH, LdotH, u_specularColor, u_specularShininess) * u_specularStrength;

       	Lo += (diffuseContrib + specContrib) * attenuation;
    }

    f_color = vec4(Lo, 1.0); 
}
