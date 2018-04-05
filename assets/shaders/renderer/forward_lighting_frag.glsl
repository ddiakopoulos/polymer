// This implementation is heavily based on the reference available for the gLTF File format: 
// https://github.com/KhronosGroup/glTF-WebGL-PBR/blob/master/shaders/pbr-frag.glsl (MIT Licensed)

// http://gamedev.stackexchange.com/questions/63832/normals-vs-normal-maps/63833
// http://blog.selfshadow.com/publications/blending-in-detail/
// http://www.trentreed.net/blog/physically-based-shading-and-image-based-lighting/
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
// http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr_v2.pdf
// http://www.thetenthplanet.de/archives/1180

// [1] http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// [2] http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
// [3] https://github.com/KhronosGroup/glTF-WebGL-PBR/#environment-maps
// [4] https://www.cs.virginia.edu/~jdl/bib/appearance/analytic%20models/schlick94b.pdf

#include "renderer_common.glsl"
#include "colorspace_conversions.glsl"
#include "cascaded_shadows.glsl"

in vec3 v_world_position;
in vec3 v_view_space_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_tangent;
in vec3 v_bitangent;

// Material Uniforms
uniform float u_roughness = 1;
uniform float u_metallic = 1;
uniform float u_opacity = 1;

uniform sampler2D s_albedo;
uniform sampler2D s_normal;
uniform sampler2D s_roughness;
uniform sampler2D s_metallic;
uniform sampler2D s_emissive;
uniform sampler2D s_height;
uniform sampler2D s_occlusion;

// Image-Based-Lighting Uniforms
uniform samplerCube sc_radiance;
uniform samplerCube sc_irradiance;

// Lighting & Shadowing Uniforms
uniform float u_pointLightAttenuation = 1.0;
uniform float u_shadowOpacity = 1.0;

// Dielectrics have an F0 between 0.2 - 0.5, often exposed as the "specular level" parameter
uniform vec3 u_albedo = vec3(1, 1, 1);
uniform vec3 u_emissive = vec3(1, 1, 1);
uniform float u_specularLevel = 0.04;
uniform float u_occlusionStrength = 1.0;
uniform float u_ambientStrength = 1.0;
uniform float u_emissiveStrength = 1.0;

uniform sampler2DArray s_csmArray;

out vec4 f_color;

struct LightingInfo
{
    float NdotL;                  // cos angle between normal and light direction
    float NdotV;                  // cos angle between normal and view direction
    float NdotH;                  // cos angle between normal and half vector
    float LdotH;                  // cos angle between light direction and half vector
    float VdotH;                  // cos angle between view direction and half vector
    float perceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
    float metalness;              // metallic value at the surface
    vec3 reflectance0;            // full reflectance color (normal incidence angle)
    vec3 reflectance90;           // reflectance color at grazing angle
    float alphaRoughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
    vec3 diffuseColor;            // color contribution from diffuse lighting
    vec3 specularColor;           // color contribution from specular lighting
};

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
// Shlick's approximation of the Fresnel factor.
vec3 specular_reflection(LightingInfo data)
{
    return data.reflectance0 + (data.reflectance90 - data.reflectance0) * pow(clamp(1.0 - data.VdotH, 0.0, 1.0), 5.0);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alphaRoughness as input as originally proposed in [2].
float geometric_occlusion(LightingInfo data)
{
    float NdotL = data.NdotL;
    float NdotV = data.NdotV;
    float r = data.alphaRoughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from Epic
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// GGX/Towbridge-Reitz normal distribution function.
// Disney's reparametrization of alpha = roughness^2.
float microfacet_distribution(LightingInfo data)
{
    float roughnessSq = data.alphaRoughness * data.alphaRoughness; // squared?
    //float denom = (data.NdotH * data.NdotH) * (roughnessSq - 1.0) + 1.0;
    //return roughnessSq / (PI * denom * denom);
    float f = (data.NdotH * roughnessSq - data.NdotH) * data.NdotH + 1.0;
    return roughnessSq / (PI * f * f);
}

void compute_cook_torrance(LightingInfo data, float attenuation, out vec3 diffuseContribution, out vec3 specularContribution)
{
    const vec3 F = specular_reflection(data);
    const float G = geometric_occlusion(data);
    const float D = microfacet_distribution(data);

    // Calculation of analytical lighting contribution
    diffuseContribution = ((1.0 - F) * (data.diffuseColor / PI)) * attenuation; 
    specularContribution = ((F * G * D) / ((4.0 * data.NdotL * data.NdotV) + 0.001)) * attenuation;
}

void main()
{   
    // Surface properties
    vec3 albedo = u_albedo;
    vec3 N = normalize(v_normal);

    float roughness = clamp(u_roughness, u_specularLevel, 1.0);
    float metallic = u_metallic;

#ifdef HAS_NORMAL_MAP
    vec3 nSample = (texture(s_normal, v_texcoord).xyz * 2.0 - 1.0);
    N = normalize(calc_normal_map(v_normal, normalize(v_tangent), normalize(v_bitangent), normalize(nSample)).xyz);
    //N = normalize( (normalize(v_tangent) * nSample.x) + (normalize(v_bitangent) * nSample.y) + (normalize(v_normal) * nSample.z)); // alternate
#endif

// todo - can pack roughness and metalness into the same RG texture
#ifdef HAS_ROUGHNESS_MAP
    roughness *= texture(s_roughness, v_texcoord).r;
#endif

 #ifdef HAS_METALNESS_MAP
    metallic *= texture(s_metallic, v_texcoord).r;
#endif

#ifdef HAS_ALBEDO_MAP
    albedo *= sRGBToLinear(texture(s_albedo, v_texcoord).rgb, DEFAULT_GAMMA); 
#endif

    roughness = geometric_aa_toksvig(roughness, nSample, 0.25);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness [2].
    const float alphaRoughness = roughness;// * roughness; @todo experiment with alpha squared

    // View direction
    vec3 V = normalize(u_eyePos.xyz - v_world_position);
    float NdotV = abs(dot(N, V)) + 0.001;

    vec3 F0 = vec3(u_specularLevel);
    vec3 diffuseColor = albedo * (vec3(1.0) - F0);
    //diffuseColor *= 1.0 - metallic;

    // Fresnel reflectance at normal incidence (for metals use albedo color).
    vec3 specularColor = mix(F0, albedo, metallic);
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
    // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
    float reflectance90 = clamp(reflectance * 25, 0.0, 1.0);
    vec3 specularEnvironmentR0 = specularColor.rgb;
    vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

    vec3 Lo = vec3(0);

    vec3 debugShadowColor;
    float shadowVisibility = 1;

    // Compute directional light
    {
        vec3 L = normalize(u_directionalLight.direction); 
        vec3 H = normalize(L + V);  
        float NdotL = clamp(dot(N, L), 0.001, 1.0);
        float NdotH = clamp(dot(N, H), 0.0, 1.0);
        float LdotH = clamp(dot(L, H), 0.0, 1.0);
        float VdotH = clamp(dot(V, H), 0.0, 1.0);

        LightingInfo data = LightingInfo(
            NdotL, NdotV, NdotH, LdotH, VdotH,
            roughness, metallic, specularEnvironmentR0, specularEnvironmentR90, alphaRoughness,
            diffuseColor, specularColor
        );

        float NdotL_S = clamp(dot(v_normal, L), 0.001, 1.0);
        const float slope_bias = 0.04;  // fixme - expose to user
        const float normal_bias = 0.01; // fixme - expose to user
        vec3 biased_pos = get_biased_position(v_world_position, slope_bias, normal_bias, v_normal, L);

        // The way this is structured, it impacts lighting if we stop updating shadow uniforms 
        float shadowTerm = 1.0;
        #ifdef ENABLE_SHADOWS
            shadowTerm = calculate_csm_coefficient(s_csmArray, biased_pos, v_view_space_position, u_cascadesMatrix, u_cascadesPlane, debugShadowColor);
            shadowVisibility = 1.0 - ((shadowTerm  * NdotL) * u_shadowOpacity * u_receiveShadow);
        #endif

        vec3 diffuseContrib, specContrib;
        compute_cook_torrance(data, u_directionalLight.amount, diffuseContrib, specContrib);

        Lo += NdotL * u_directionalLight.color * (diffuseContrib + specContrib);
    }

    // Compute point lights
    for (int i = 0; i < u_activePointLights; ++i)
    {
        vec3 L = normalize(u_pointLights[i].position - v_world_position); 
        vec3 H = normalize(L + V);  

        float NdotL = clamp(dot(N, L), 0.001, 1.0);
        float NdotH = clamp(dot(N, H), 0.0, 1.0);
        float LdotH = clamp(dot(L, H), 0.0, 1.0);
        float VdotH = clamp(dot(V, H), 0.0, 1.0);

        LightingInfo data = LightingInfo(
            NdotL, NdotV, NdotH, LdotH, VdotH,
            roughness, metallic, specularEnvironmentR0, specularEnvironmentR90, alphaRoughness,
            diffuseColor, specularColor
        );

        float dist = length(u_pointLights[i].position - v_world_position);
        float attenuation = point_light_attenuation(u_pointLights[i].radius, 2.0, 0.1, dist); // reasonable intensity is 0.01 to 8

        vec3 diffuseContrib, specContrib;
        compute_cook_torrance(data, attenuation, diffuseContrib, specContrib);

        Lo += NdotL * u_pointLights[i].color * (diffuseContrib + specContrib) * attenuation;
    }

    #ifdef USE_IMAGE_BASED_LIGHTING
    {
        const int NUM_MIP_LEVELS = 8;
        float mipLevel = NUM_MIP_LEVELS - 1.0 + log2(roughness);
        vec3 cubemapLookup = fix_cube_lookup(-reflect(V, N), 512, mipLevel);

        vec3 irradiance = sRGBToLinear(texture(sc_irradiance, N).rgb, DEFAULT_GAMMA) * u_ambientStrength;
        vec3 radiance = sRGBToLinear(textureLod(sc_radiance, cubemapLookup, mipLevel).rgb, DEFAULT_GAMMA) * u_ambientStrength;

        vec3 environment_reflectance = env_brdf_approx(specularColor, pow(roughness, 4.0), NdotV);

        vec3 iblDiffuse = (diffuseColor * irradiance);
        vec3 iblSpecular = (environment_reflectance * radiance);

        Lo += (iblDiffuse + iblSpecular);
    }
    #endif

    #ifdef HAS_EMISSIVE_MAP
        Lo += texture(s_emissive, v_texcoord).rgb * u_emissiveStrength; 
    #endif

    #ifdef HAS_OCCLUSION_MAP
        float ao = texture(s_occlusion, v_texcoord).r;
        Lo = mix(Lo, Lo * ao, u_occlusionStrength);
    #endif

    Lo += u_emissive * u_emissiveStrength;

    // Debugging
    //f_color = vec4(vec3(debugShadowColor), 1.0);
    //f_color = vec4(mix(vec3(shadowVisibility), vec3(debugShadowColor), 0.5), 1.0);
    //f_color = vec4(vec3(shadowVisibility), u_opacity); 
    //f_color = vec4(nSample, 1.0);
    //f_color = vec4(vec3(roughness), 1.0);

    // Combine direct lighting, IBL, and shadow visbility
    f_color = vec4(Lo * shadowVisibility, u_opacity); 
}
