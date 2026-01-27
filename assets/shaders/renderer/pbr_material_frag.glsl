// This implementation is heavily based on the reference available for the gLTF File format: 
// https://github.com/KhronosGroup/glTF-WebGL-PBR/blob/master/shaders/pbr-frag.glsl (MIT Licensed)

// [1] http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// [2] http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
// [3] https://github.com/KhronosGroup/glTF-WebGL-PBR/#environment-maps
// [4] https://www.cs.virginia.edu/~jdl/bib/appearance/analytic%20models/schlick94b.pdf
// [5] https://developer.nvidia.com/gpugems/GPUGems/gpugems_ch19.html

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

#ifdef HAS_ALBEDO_MAP
    uniform sampler2D s_albedo;
#endif

#ifdef HAS_NORMAL_MAP
    uniform sampler2D s_normal;
#endif

#ifdef HAS_ROUGHNESS_MAP
    uniform sampler2D s_roughness;
#endif

#ifdef HAS_METALNESS_MAP
    uniform sampler2D s_metallic;
#endif

#ifdef HAS_EMISSIVE_MAP
    uniform sampler2D s_emissive;
#endif

#ifdef HAS_OCCLUSION_MAP
    uniform sampler2D s_occlusion;
#endif

// Lighting & Shadowing Uniforms
uniform float u_pointLightAttenuation = 1.0;
uniform float u_shadowOpacity = 1.0;

// Dielectrics have an F0 between 0.2 - 0.5, often exposed as the "specular level" parameter
uniform vec3 u_albedo = vec3(1, 1, 1);
uniform vec3 u_emissive = vec3(1, 1, 1);
uniform float u_specularLevel = 0.04;
uniform float u_reflectance = 0.5;
uniform float u_occlusionStrength = 1.0;
uniform float u_ambientStrength = 1.0;
uniform float u_emissiveStrength = 1.0;

// Clear coat layer parameters (Filament specification)
uniform float u_clearCoat = 0.0;
uniform float u_clearCoatRoughness = 0.0;

#ifdef ENABLE_SHADOWS
    uniform sampler2DArray s_csmArray;
#endif

// Image-Based-Lighting Uniforms
#ifdef USE_IMAGE_BASED_LIGHTING
    uniform samplerCube sc_irradiance;
    uniform samplerCube sc_radiance;
    uniform sampler2D s_dfg_lut;
#endif

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

// Height-correlated Smith-GGX visibility function (Filament specification)
// This is the V(l,v) term that combines G(l,v) / (4 * NdotL * NdotV)
float visibility_smith_ggx_correlated(float NdotV, float NdotL, float roughness)
{
    float a2 = roughness * roughness;
    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL);
}

float geometric_occlusion(LightingInfo data)
{
    return visibility_smith_ggx_correlated(data.NdotV, data.NdotL, data.alphaRoughness);
}
  
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from Epic
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// GGX/Towbridge-Reitz normal distribution function.
// Disney's reparametrization of alpha = roughness^2.
float microfacet_distribution(LightingInfo data)
{
    float roughnessSq = data.alphaRoughness * data.alphaRoughness;
    float f = (data.NdotH * roughnessSq - data.NdotH) * data.NdotH + 1.0;
    return roughnessSq / (PI * f * f);
}

// GGX distribution for clear coat (takes alpha directly)
float D_GGX(float NdotH, float a)
{
    float a2 = a * a;
    float f = (NdotH * a2 - NdotH) * NdotH + 1.0;
    return a2 / (PI * f * f);
}

// Kelemen visibility function for clear coat (Filament specification)
// Cheaper than Smith-GGX, appropriate for the smooth clear coat layer
float visibility_kelemen(float LdotH)
{
    return 0.25 / (LdotH * LdotH);
}

void compute_cook_torrance(LightingInfo data, float attenuation, out vec3 diffuseContribution, out vec3 specularContribution)
{
    const vec3 F = specular_reflection(data);
    const float V = geometric_occlusion(data);
    const float D = microfacet_distribution(data);

    // Calculation of analytical lighting contribution
    // Note: V already includes the 1/(4*NdotL*NdotV) term (height-correlated visibility)
    diffuseContribution = ((1.0 - F) * (data.diffuseColor / PI)) * attenuation;
    specularContribution = (F * V * D) * attenuation;
}

void main()
{   
    // Surface properties
    vec3 albedo = u_albedo;
    vec3 N = normalize(v_normal);

    float roughness = clamp(u_roughness, 0.089, 1.0);
    float metallic = u_metallic;

#ifdef HAS_NORMAL_MAP
    vec3 nSample = normalize(texture(s_normal, v_texcoord).xyz * 2.0 - 1.0);
    N = normalize(calc_normal_map(v_normal, normalize(v_tangent), normalize(v_bitangent), normalize(nSample)).xyz);
#endif

#ifdef HAS_ROUGHNESS_MAP
    roughness = texture(s_roughness, v_texcoord).r * roughness;
#endif

#ifdef HAS_METALNESS_MAP
    metallic = texture(s_metallic, v_texcoord).r * metallic;
#endif

#ifdef HAS_ALBEDO_MAP
    albedo *= sRGBToLinear(texture(s_albedo, v_texcoord).rgb, DEFAULT_GAMMA); 
#endif

//#ifdef HAS_NORMAL_MAP
//    roughness = geometric_aa_toksvig(roughness, nSample, 0.5);
//#endif

    metallic = clamp(metallic, 0.0, 1.0);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness [2].
    const float alphaRoughness = roughness * roughness; //@todo experiment with alpha squared

    // View direction
    vec3 V = normalize(u_eyePos.xyz - v_world_position);
    float NdotV = abs(dot(N, V)) + 0.001;

    // F0 calculation using Filament's reflectance parameter (0.16 * reflectance^2 maps to 4% for reflectance=0.5)
    // For dielectrics: F0 = 0.16 * reflectance^2 (default 0.5 gives 4%)
    // For metals: F0 = albedo (metal color is its specular color)
    vec3 F0 = 0.16 * u_reflectance * u_reflectance * (1.0 - metallic) + albedo * metallic;

    // Diffuse color: metals have no diffuse, dielectrics use albedo
    // The (1-F) Fresnel term is applied dynamically in the BRDF, not here
    vec3 diffuseColor = (1.0 - metallic) * albedo;

    // Specular color is F0 (used for Fresnel calculations)
    vec3 specularColor = F0;
    float reflectanceLuminance = max(max(specularColor.r, specularColor.g), specularColor.b);

    // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
    // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflectance to 0%.
    float reflectance90 = clamp(reflectanceLuminance * 25, 0.0, 1.0);
    vec3 specularEnvironmentR0 = specularColor.rgb;
    vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

    // Separate lighting contributions for proper shadow application
    vec3 Lo_direct = vec3(0, 0, 0);
    vec3 Lo_indirect = vec3(0, 0, 0);
    vec3 Lo_emissive = vec3(0, 0, 0);

    vec3 debugShadowColor;
    float shadowVisibility = 1.0;

    // Compute directional light (direct lighting)
    if (sunlightActive > 0)
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
        const float slope_bias = 0.04;
        const float normal_bias = 0.01;
        vec3 biased_pos = get_biased_position(v_world_position, slope_bias, normal_bias, v_normal, L);

        float shadowTerm = 1.0;
        #ifdef ENABLE_SHADOWS
            shadowTerm = calculate_csm_coefficient(s_csmArray, biased_pos, v_view_space_position, u_cascadesMatrix, u_cascadesPlane, debugShadowColor);
            shadowVisibility = 1.0 - ((shadowTerm * NdotL) * u_shadowOpacity * u_receiveShadow);
        #endif

        vec3 diffuseContrib, specContrib;
        compute_cook_torrance(data, u_directionalLight.amount, diffuseContrib, specContrib);

        Lo_direct += NdotL * u_directionalLight.color * (diffuseContrib + specContrib);
    }

    // Compute point lights (direct lighting)
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
        float attenuation = point_light_attenuation(u_pointLights[i].radius, 2.0, 0.1, dist);

        vec3 diffuseContrib, specContrib;
        compute_cook_torrance(data, attenuation, diffuseContrib, specContrib);

        Lo_direct += NdotL * u_pointLights[i].color * (diffuseContrib + specContrib) * attenuation;
    }

    // Image-based lighting (indirect lighting - not affected by shadows)
    #ifdef USE_IMAGE_BASED_LIGHTING
    {
        const int NUM_MIP_LEVELS = 6;
        float mipLevel = NUM_MIP_LEVELS - 1 + log2(roughness);
        vec3 cubemapLookup = fix_cube_lookup(-reflect(V, N), 512, mipLevel);

        vec3 irradiance = sRGBToLinear(texture(sc_irradiance, N).rgb, DEFAULT_GAMMA) * u_ambientStrength;
        vec3 radiance = sRGBToLinear(textureLod(sc_radiance, cubemapLookup, mipLevel).rgb, DEFAULT_GAMMA) * u_ambientStrength;

        // DFG LUT lookup for accurate split-sum approximation
        vec2 dfg = texture(s_dfg_lut, vec2(NdotV, roughness)).rg;
        vec3 specularDFG = specularColor * dfg.x + dfg.y;

        // Energy compensation for multiscatter (Filament specification)
        // Clamped to prevent blowup when dfg.y is very small
        float energyBias = max(dfg.y, 0.1);
        vec3 energyCompensation = 1.0 + specularColor * (1.0 / energyBias - 1.0);

        vec3 iblDiffuse = diffuseColor * irradiance;
        vec3 iblSpecular = specularDFG * radiance * energyCompensation;

        Lo_indirect += iblDiffuse + iblSpecular;
    }
    #endif

    // Emissive contribution (not affected by shadows)
    #ifdef HAS_EMISSIVE_MAP
        Lo_emissive += texture(s_emissive, v_texcoord).rgb * u_emissiveStrength;
    #endif

    Lo_emissive += u_emissive * u_emissiveStrength;

    // Apply ambient occlusion to indirect lighting only
    #ifdef HAS_OCCLUSION_MAP
        float ao = texture(s_occlusion, v_texcoord).r;
        Lo_indirect = mix(Lo_indirect, Lo_indirect * ao, u_occlusionStrength);
    #endif

    // Clear coat layer (Filament specification)
    vec3 Lo_clearCoat = vec3(0.0);
    float clearCoatAttenuation = 1.0;

    if (u_clearCoat > 0.0)
    {
        float ccRoughness = clamp(u_clearCoatRoughness, 0.089, 1.0);
        float ccAlpha = ccRoughness * ccRoughness;

        // Clear coat uses polyurethane IOR ~1.5, giving F0 = 0.04
        float ccF0 = 0.04;

        // Process directional light clear coat
        if (sunlightActive > 0)
        {
            vec3 L = normalize(u_directionalLight.direction);
            vec3 H = normalize(L + V);
            float NdotL = clamp(dot(N, L), 0.001, 1.0);
            float NdotH = clamp(dot(N, H), 0.0, 1.0);
            float LdotH = clamp(dot(L, H), 0.0, 1.0);
            float VdotH = clamp(dot(V, H), 0.0, 1.0);

            float ccD = D_GGX(NdotH, ccAlpha);
            float ccV = visibility_kelemen(LdotH);
            float ccF = ccF0 + (1.0 - ccF0) * pow(1.0 - VdotH, 5.0);

            Lo_clearCoat += u_directionalLight.color * u_directionalLight.amount * NdotL * ccD * ccV * ccF * u_clearCoat;
        }

        // Process point lights clear coat
        for (int i = 0; i < u_activePointLights; ++i)
        {
            vec3 L = normalize(u_pointLights[i].position - v_world_position);
            vec3 H = normalize(L + V);
            float NdotL = clamp(dot(N, L), 0.001, 1.0);
            float NdotH = clamp(dot(N, H), 0.0, 1.0);
            float LdotH = clamp(dot(L, H), 0.0, 1.0);
            float VdotH = clamp(dot(V, H), 0.0, 1.0);

            float dist = length(u_pointLights[i].position - v_world_position);
            float attenuation = point_light_attenuation(u_pointLights[i].radius, 2.0, 0.1, dist);

            float ccD = D_GGX(NdotH, ccAlpha);
            float ccV = visibility_kelemen(LdotH);
            float ccF = ccF0 + (1.0 - ccF0) * pow(1.0 - VdotH, 5.0);

            Lo_clearCoat += u_pointLights[i].color * attenuation * NdotL * ccD * ccV * ccF * u_clearCoat;
        }

        // Fresnel attenuation of base layer (clear coat absorbs some light)
        float ccFresnel = ccF0 + (1.0 - ccF0) * pow(1.0 - NdotV, 5.0);
        clearCoatAttenuation = 1.0 - ccFresnel * u_clearCoat;
    }

    // Combine: shadows affect direct lighting, clear coat is added on top
    vec3 finalColor = (Lo_direct * clearCoatAttenuation + Lo_clearCoat) * shadowVisibility + Lo_indirect * clearCoatAttenuation + Lo_emissive;
    f_color = vec4(finalColor, u_opacity);
}
