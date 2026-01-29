// PBR Translucent Thin-Walled Bubble Material
// Dielectric Fresnel (Schlick, F0 from IOR), specular reflection from prefiltered environment,
// screen-space refraction for transmission, and optional thin-film iridescence.

#include "renderer_common.glsl"
#include "colorspace_conversions.glsl"

in vec3 v_world_position;
in vec3 v_view_space_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_tangent;
in vec3 v_bitangent;

// Material uniforms
uniform float u_ior = 1.33;
uniform float u_roughness = 0.05;
uniform float u_transmission = 0.95;
uniform float u_refractionScale = 0.02;
uniform vec3 u_tintColor = vec3(0.95, 0.98, 1.0);
uniform float u_absorptionDistance = 2.0;
uniform float u_normalStrength = 1.0;
uniform float u_opacity = 1.0;

// Thin-film iridescence uniforms
uniform float u_filmThicknessNm = 380.0;
uniform float u_filmIor = 1.3;
uniform float u_iridescenceStrength = 0.5;
uniform float u_iridescenceNoiseScale = 1.0;
uniform float u_iridescenceNoiseSpeed = 0.5;

#ifdef HAS_NORMAL_MAP
    uniform sampler2D s_normal;
#endif

#ifdef HAS_THICKNESS_MAP
    uniform sampler2D s_thickness;
#endif

#ifdef USE_IMAGE_BASED_LIGHTING
    uniform samplerCube sc_irradiance;
    uniform samplerCube sc_radiance;
    uniform sampler2D s_dfg_lut;
#endif

#ifdef USE_SCREEN_SPACE_REFRACTION
    uniform sampler2D s_sceneColor;
    uniform sampler2D s_sceneDepth;
    uniform vec2 u_screenResolution;
#endif

out vec4 f_color;

// Compute F0 from IOR using Fresnel equations for dielectrics
float ior_to_f0(float ior)
{
    float r = (ior - 1.0) / (ior + 1.0);
    return r * r;
}

// Schlick Fresnel approximation
float fresnel_schlick(float cos_theta, float f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - cos_theta, 5.0);
}

vec3 fresnel_schlick_vec(float cos_theta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - cos_theta, 5.0);
}

// GGX distribution
float D_GGX(float NdotH, float alpha)
{
    float a2 = alpha * alpha;
    float f = (NdotH * a2 - NdotH) * NdotH + 1.0;
    return a2 / (PI * f * f);
}

// Height-correlated Smith-GGX visibility
float V_SmithGGXCorrelated(float NdotV, float NdotL, float alpha)
{
    float a2 = alpha * alpha;
    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL + 0.0001);
}

// Simple 3D noise for animated film thickness variation
float hash(vec3 p)
{
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise3d(vec3 x)
{
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);

    return mix(mix(mix(hash(i + vec3(0, 0, 0)), hash(i + vec3(1, 0, 0)), f.x),
                   mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x),
                   mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

float fbm(vec3 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; i++)
    {
        value += amplitude * noise3d(p);
        p *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

// Attempt to evaluate spectral power at a given wavelength (simplified for RGB)
// Maps wavelength to RGB based on CIE 1931 approximation
vec3 wavelength_to_rgb(float wavelength_nm)
{
    float w = wavelength_nm;
    vec3 rgb;

    if (w < 380.0) rgb = vec3(0.0);
    else if (w < 440.0) rgb = vec3(-(w - 440.0) / 60.0, 0.0, 1.0);
    else if (w < 490.0) rgb = vec3(0.0, (w - 440.0) / 50.0, 1.0);
    else if (w < 510.0) rgb = vec3(0.0, 1.0, -(w - 510.0) / 20.0);
    else if (w < 580.0) rgb = vec3((w - 510.0) / 70.0, 1.0, 0.0);
    else if (w < 645.0) rgb = vec3(1.0, -(w - 645.0) / 65.0, 0.0);
    else if (w < 780.0) rgb = vec3(1.0, 0.0, 0.0);
    else rgb = vec3(0.0);

    // Intensity correction at spectrum edges
    float factor = 1.0;
    if (w < 420.0) factor = 0.3 + 0.7 * (w - 380.0) / 40.0;
    else if (w > 700.0) factor = 0.3 + 0.7 * (780.0 - w) / 80.0;

    return rgb * factor;
}

// Compute thin-film Fresnel reflectance with interference
// Returns spectral F0 modification (RGB) that should multiply the base Fresnel
vec3 thin_film_fresnel(float cos_theta, float film_thickness_nm, float film_ior, float base_ior, float base_f0)
{
    // Snell's law for angle in film
    float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));
    float sin_theta_film = clamp(sin_theta / film_ior, -1.0, 1.0);
    float cos_theta_film = sqrt(max(0.0, 1.0 - sin_theta_film * sin_theta_film));

    // Fresnel amplitude coefficients at interfaces (s-polarization approximation)
    // Interface 0->1 (air to film)
    float n0 = 1.0;
    float n1 = film_ior;
    float n2 = base_ior;

    float r01 = (n0 * cos_theta - n1 * cos_theta_film) / (n0 * cos_theta + n1 * cos_theta_film);
    float r12 = (n1 * cos_theta_film - n2 * cos_theta) / (n1 * cos_theta_film + n2 * cos_theta);

    // Sample multiple wavelengths and accumulate to RGB
    vec3 reflectance = vec3(0.0);
    const int NUM_SAMPLES = 16;
    float wavelength_start = 380.0;
    float wavelength_end = 780.0;

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        float t = (float(i) + 0.5) / float(NUM_SAMPLES);
        float wavelength = mix(wavelength_start, wavelength_end, t);

        // Optical path difference (in nm)
        float opd = 2.0 * n1 * film_thickness_nm * cos_theta_film;

        // Phase shift (with pi phase shift at one interface for dielectric)
        float phase = 2.0 * PI * opd / wavelength + PI;

        // Airy formula for thin-film reflectance
        // R = (r01^2 + r12^2 + 2*r01*r12*cos(phase)) / (1 + r01^2*r12^2 + 2*r01*r12*cos(phase))
        float r01_sq = r01 * r01;
        float r12_sq = r12 * r12;
        float cos_phase = cos(phase);

        float numerator = r01_sq + r12_sq + 2.0 * r01 * r12 * cos_phase;
        float denominator = 1.0 + r01_sq * r12_sq + 2.0 * r01 * r12 * cos_phase;
        float R = numerator / max(denominator, 0.001);

        // Accumulate weighted by wavelength sensitivity
        vec3 rgb_weight = wavelength_to_rgb(wavelength);
        reflectance += rgb_weight * R;
    }

    // Normalize by total RGB weight
    vec3 total_weight = vec3(0.0);
    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        float t = (float(i) + 0.5) / float(NUM_SAMPLES);
        float wavelength = mix(wavelength_start, wavelength_end, t);
        total_weight += wavelength_to_rgb(wavelength);
    }
    reflectance /= max(total_weight, vec3(0.001));

    // Scale to be usable as a Fresnel multiplier (normalize around base_f0)
    // This gives us colored reflectance that varies with viewing angle
    return clamp(reflectance * 2.5, 0.0, 1.0);
}

void main()
{
    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_eyePos.xyz - v_world_position);

    // Normal mapping
    #ifdef HAS_NORMAL_MAP
    {
        vec3 normal_sample = texture(s_normal, v_texcoord).xyz * 2.0 - 1.0;
        normal_sample.xy *= u_normalStrength;
        normal_sample = normalize(normal_sample);
        N = normalize(calc_normal_map(v_normal, normalize(v_tangent), normalize(v_bitangent), normal_sample).xyz);
    }
    #endif

    float NdotV = max(dot(N, V), 0.001);

    // Dielectric Fresnel from IOR
    float f0 = ior_to_f0(u_ior);

    // Roughness for specular
    float roughness = clamp(u_roughness, 0.045, 1.0);
    float alpha = roughness * roughness;

    vec3 reflection_color = vec3(0.0);
    vec3 refraction_color = vec3(0.0);

    // Calculate film thickness with optional animated noise
    float film_thickness = u_filmThicknessNm;

    #ifdef HAS_THICKNESS_MAP
        film_thickness *= texture(s_thickness, v_texcoord).r;
    #endif

    // Animated noise variation for iridescence
    if (u_iridescenceStrength > 0.0)
    {
        vec3 noise_coord = v_world_position * u_iridescenceNoiseScale + vec3(0.0, 0.0, u_time * u_iridescenceNoiseSpeed);
        float thickness_variation = fbm(noise_coord) * 0.5 + 0.5;
        film_thickness *= mix(0.7, 1.3, thickness_variation);
    }

    // Compute thin-film spectral Fresnel (RGB F0 modification)
    vec3 thin_film_F0 = thin_film_fresnel(NdotV, film_thickness, u_filmIor, u_ior, f0);

    // Blend between base F0 and thin-film F0 based on iridescence strength
    vec3 effective_F0 = mix(vec3(f0), thin_film_F0, u_iridescenceStrength);

    // Scalar fresnel for transmission calculations (use luminance of effective F0)
    float fresnel = fresnel_schlick(NdotV, dot(effective_F0, vec3(0.2126, 0.7152, 0.0722)));

    // Image-based lighting for reflection
    #ifdef USE_IMAGE_BASED_LIGHTING
    {
        const int NUM_MIP_LEVELS = 6;
        float mip_level = NUM_MIP_LEVELS - 1.0 + log2(roughness);
        vec3 R = reflect(-V, N);
        vec3 cubemap_lookup = fix_cube_lookup(R, 512.0, mip_level);

        vec3 radiance = sRGBToLinear(textureLod(sc_radiance, cubemap_lookup, mip_level).rgb, DEFAULT_GAMMA);

        // DFG lookup for specular with thin-film colored F0
        vec2 dfg = texture(s_dfg_lut, vec2(NdotV, roughness)).rg;
        vec3 specular_dfg = effective_F0 * dfg.x + dfg.y;

        reflection_color = radiance * specular_dfg;
    }
    #endif

    // Screen-space refraction
    #ifdef USE_SCREEN_SPACE_REFRACTION
    {
        // Calculate refraction direction using Snell's law approximation
        float eta = 1.0 / u_ior;
        vec3 refract_dir = refract(-V, N, eta);

        // Project refraction offset to screen space
        vec2 screen_uv = gl_FragCoord.xy / u_screenResolution;

        // Offset based on normal and refraction scale
        vec2 refract_offset = N.xy * u_refractionScale;
        refract_offset *= (1.0 - NdotV); // Stronger at grazing angles

        vec2 refract_uv = screen_uv + refract_offset;
        refract_uv = clamp(refract_uv, vec2(0.001), vec2(0.999));

        // Sample scene color at refracted position
        vec3 scene_color = texture(s_sceneColor, refract_uv).rgb;

        // Apply tint and absorption
        float path_length = 1.0 / max(NdotV, 0.1);
        vec3 absorption = exp(-path_length / u_absorptionDistance * (1.0 - u_tintColor));

        refraction_color = scene_color * u_tintColor * absorption;
    }
    #else
    {
        // Fallback: use irradiance for transmission approximation
        #ifdef USE_IMAGE_BASED_LIGHTING
            vec3 irradiance = sRGBToLinear(texture(sc_irradiance, -N).rgb, DEFAULT_GAMMA);
            refraction_color = irradiance * u_tintColor;
        #else
            refraction_color = u_tintColor * 0.5;
        #endif
    }
    #endif

    // Direct lighting contribution (specular only for bubbles)
    // Use spectral (RGB) Fresnel from thin-film for colored specular highlights
    vec3 direct_specular = vec3(0.0);

    if (sunlightActive > 0)
    {
        vec3 L = normalize(u_directionalLight.direction);
        vec3 H = normalize(L + V);

        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float VdotH = max(dot(V, H), 0.0);

        float D = D_GGX(NdotH, alpha);
        float Vis = V_SmithGGXCorrelated(NdotV, NdotL, alpha);

        // Use vectorized Fresnel with thin-film F0 for colored specular
        vec3 F = fresnel_schlick_vec(VdotH, effective_F0);

        vec3 spec = (D * Vis) * F * u_directionalLight.color * u_directionalLight.amount * NdotL;

        direct_specular += spec;
    }

    // Point lights
    for (int i = 0; i < u_activePointLights; ++i)
    {
        vec3 L = normalize(u_pointLights[i].position - v_world_position);
        vec3 H = normalize(L + V);

        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float VdotH = max(dot(V, H), 0.0);

        float dist = length(u_pointLights[i].position - v_world_position);
        float attenuation = point_light_attenuation(u_pointLights[i].radius, 2.0, 0.1, dist);

        float D = D_GGX(NdotH, alpha);
        float Vis = V_SmithGGXCorrelated(NdotV, NdotL, alpha);

        // Use vectorized Fresnel with thin-film F0 for colored specular
        vec3 F = fresnel_schlick_vec(VdotH, effective_F0);

        vec3 spec = (D * Vis) * F * u_pointLights[i].color * attenuation * NdotL;

        direct_specular += spec;
    }

    // Combine reflection and refraction based on Fresnel and transmission
    float effective_fresnel = fresnel;
    float transmission_factor = u_transmission * (1.0 - effective_fresnel);
    float reflection_factor = effective_fresnel + (1.0 - u_transmission) * (1.0 - effective_fresnel);

    vec3 final_color = reflection_color * reflection_factor + refraction_color * transmission_factor + direct_specular;

    // Apply opacity
    float final_alpha = u_opacity * (reflection_factor + transmission_factor * 0.5);
    final_alpha = clamp(final_alpha, 0.0, 1.0);

    f_color = vec4(final_color, final_alpha);
}
