#include "renderer_common.glsl"

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

void main()
{   
    // Surface properties
    vec3 diffuseColor = u_diffuseColor;
    vec3 N = normalize(v_normal);

#ifdef HAS_NORMAL_MAP
    vec3 nSample = normalize(texture(s_normal, v_texcoord).xyz * 2.0 - 1.0);
    N = normalize(calc_normal_map(v_normal, normalize(v_tangent), normalize(v_bitangent), normalize(nSample)).xyz);
#endif

#ifdef HAS_DIFFUSE_MAP
    diffuseColor *= sRGBToLinear(texture(s_diffuse, v_texcoord).rgb, DEFAULT_GAMMA); 
#endif

    // View direction
    vec3 V = normalize(u_eyePos.xyz - v_world_position);
    float NdotV = abs(dot(N, V)) + 0.001;

    vec3 Lo = vec3(0, 0, 0);

    vec3 debugShadowColor;
    float shadowVisibility = 1;

    // Compute directional light
    {
        vec3 L = normalize(u_directionalLight.direction); 
        vec3 H = normalize(L + V);  
        float NdotL = clamp(dot(N, L), 0.001, 1.0);

        vec3 diffuseContrib, specContrib;

        // todo

        Lo *= NdotL * u_directionalLight.color * (diffuseContrib + specContrib);
    }

    // Compute point lights
    for (int i = 0; i < u_activePointLights; ++i)
    {
        vec3 L = normalize(u_pointLights[i].position - v_world_position); 
        vec3 H = normalize(L + V);  

        float NdotL = clamp(dot(N, L), 0.001, 1.0);

        float dist = length(u_pointLights[i].position - v_world_position);
        float attenuation = point_light_attenuation(u_pointLights[i].radius, 2.0, 0.1, dist); // reasonable intensity is 0.01 to 8

        vec3 diffuseContrib, specContrib;

        // todo 

        Lo *= NdotL * u_pointLights[i].color * (diffuseContrib + specContrib) * attenuation;
    }
    
    f_color = vec4(Lo, 1.0); 
}
