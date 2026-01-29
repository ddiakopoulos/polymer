////////////////////////////////////////////////////////////////////////////////
// PBR Material Fragment Shader
////////////////////////////////////////////////////////////////////////////////
// This shader implements a Physically Based Rendering (PBR) material model based
// on the Cook-Torrance microfacet BRDF. The implementation follows principles from:
// [1] Epic Games - "Real Shading in Unreal Engine 4" (Karis, SIGGRAPH 2013)
//     http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// [2] Disney - "Physically-Based Shading at Disney" (Burley, SIGGRAPH 2012)
//     http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
// [3] Khronos glTF PBR Reference
//     https://github.com/KhronosGroup/glTF-WebGL-PBR/#environment-maps
// [4] Schlick Fresnel Approximation
//     https://www.cs.virginia.edu/~jdl/bib/appearance/analytic%20models/schlick94b.pdf
// [5] GPU Gems - Image-Based Lighting
//     https://developer.nvidia.com/gpugems/GPUGems/gpugems_ch19.html
// [6] Google Filament PBR Documentation
//     https://google.github.io/filament/Filament.html
//
// BRDF MODEL OVERVIEW:
// ====================
// The surface response is modeled as:
//     f(v,l) = f_d(v,l) + f_r(v,l)
//
// Where:
//   f_d = Diffuse BRDF (Lambertian)
//   f_r = Specular BRDF (Cook-Torrance microfacet model)
//
// The Cook-Torrance specular BRDF is:
//     f_r(v,l) = D(h,α) * G(v,l,α) * F(v,h,f0) / (4 * NdotV * NdotL)
//
// Where:
//   D = Normal Distribution Function (GGX/Trowbridge-Reitz)
//   G = Geometric Shadowing/Masking Function (Smith-GGX height-correlated)
//   F = Fresnel Reflectance (Schlick approximation)
//
// NOTATION:
// =========
//   N     = Surface normal unit vector
//   V     = View direction unit vector (surface to eye)
//   L     = Light direction unit vector (surface to light)
//   H     = Half-vector = normalize(V + L)
//   α     = Roughness squared (perceptualRoughness²)
//   f0    = Fresnel reflectance at normal incidence
//   f90   = Fresnel reflectance at grazing angle (typically 1.0)
//   NdotL = Clamped dot product of N and L, represents cos(θ_l)
//   NdotV = Clamped dot product of N and V, represents cos(θ_v)
//   NdotH = Clamped dot product of N and H
//   VdotH = Clamped dot product of V and H (equals LdotH)
//
////////////////////////////////////////////////////////////////////////////////

#include "renderer_common.glsl"
#include "colorspace_conversions.glsl"
#include "cascaded_shadows.glsl"

////////////////////////////////////////////////////////////////////////////////
// VERTEX SHADER INPUTS (Interpolated)
////////////////////////////////////////////////////////////////////////////////

in vec3 v_world_position;       // Fragment position in world space
in vec3 v_view_space_position;  // Fragment position in view/camera space (for CSM)
in vec3 v_normal;               // Interpolated vertex normal (may not be normalized)
in vec2 v_texcoord;             // Texture coordinates for material maps
in vec3 v_tangent;              // Tangent vector for normal mapping (TBN matrix)
in vec3 v_bitangent;            // Bitangent vector for normal mapping (TBN matrix)

////////////////////////////////////////////////////////////////////////////////
// MATERIAL UNIFORMS
////////////////////////////////////////////////////////////////////////////////

// Base material properties
// Note: These are multiplied with texture samples when maps are present
uniform float u_roughness = 1;   // Perceptual roughness [0,1]. Controls specular highlight sharpness.
uniform float u_metallic = 1;    // Metalness [0,1]. Binary in practice (0=dielectric, 1=metal).
uniform float u_opacity = 1;     // Surface opacity for transparency

// Texture samplers for PBR material maps
// Each map is conditionally compiled based on material requirements
#ifdef HAS_ALBEDO_MAP
    uniform sampler2D s_albedo;      // Base color/albedo in sRGB space
#endif

#ifdef HAS_NORMAL_MAP
    uniform sampler2D s_normal;      // Tangent-space normal map
#endif

#ifdef HAS_ROUGHNESS_MAP
    uniform sampler2D s_roughness;   // Roughness map (typically in green channel of ORM)
#endif

#ifdef HAS_METALNESS_MAP
    uniform sampler2D s_metallic;    // Metalness map (typically in blue channel of ORM)
#endif

#ifdef HAS_EMISSIVE_MAP
    uniform sampler2D s_emissive;    // Emissive color map
#endif

#ifdef HAS_OCCLUSION_MAP
    uniform sampler2D s_occlusion;   // Ambient occlusion map (typically in red channel of ORM)
#endif

////////////////////////////////////////////////////////////////////////////////
// LIGHTING & SHADOWING UNIFORMS
////////////////////////////////////////////////////////////////////////////////

uniform float u_pointLightAttenuation = 1.0;  // Global point light attenuation multiplier
uniform float u_shadowOpacity = 1.0;          // Shadow darkness [0,1]. 0=no shadows, 1=full shadows

////////////////////////////////////////////////////////////////////////////////
// MATERIAL APPEARANCE UNIFORMS
////////////////////////////////////////////////////////////////////////////////

// Base color: For dielectrics this is diffuse albedo, for metals it's specular F0
uniform vec3 u_albedo = vec3(1, 1, 1);

// Emissive color: Added to final output, useful for glowing materials
uniform vec3 u_emissive = vec3(1, 1, 1);

// DEPRECATED: specularLevel is superseded by reflectance parameter
// Kept for backwards compatibility. Dielectrics typically have F0 of 0.02-0.05 (2-5% reflectance)
uniform float u_specularLevel = 0.04;

// Reflectance parameter: 
// Controls F0 for dielectrics via: F0 = 0.16 * reflectance²
// - reflectance = 0.5 → F0 = 0.04 (4%, typical for plastics, glass)
// - reflectance = 0.35 → F0 = 0.02 (2%, water, fabric)
// - reflectance = 1.0 → F0 = 0.16 (16%, gemstones)
uniform float u_reflectance = 0.5;

// Strength multipliers for various lighting contributions
uniform float u_occlusionStrength = 1.0;   // How much AO affects indirect lighting [0,1]
uniform float u_ambientStrength = 1.0;     // IBL intensity multiplier
uniform float u_emissiveStrength = 1.0;    // Emissive intensity multiplier

////////////////////////////////////////////////////////////////////////////////
// CLEAR COAT LAYER UNIFORMS
////////////////////////////////////////////////////////////////////////////////
// Clear coat models a thin, transparent dielectric layer over the base material.
// Common examples: car paint, lacquered wood, acrylic coatings, varnish.
//
// The clear coat layer uses a simplified Cook-Torrance BRDF:
//   - GGX NDF (same as base layer)
//   - Kelemen visibility function (cheaper than Smith-GGX)
//   - Fixed F0 = 0.04 (polyurethane IOR ≈ 1.5)
//
// Energy conservation: The base layer is attenuated by (1 - Fc) where Fc is
// the clear coat Fresnel term, accounting for light absorbed by the clear coat.

uniform float u_clearCoat = 0.0;           // Clear coat intensity [0,1]. 0=disabled.
uniform float u_clearCoatRoughness = 0.0;  // Clear coat roughness [0,1]. Usually very low (0.0-0.2).

////////////////////////////////////////////////////////////////////////////////
// SHADOW MAPPING UNIFORMS
////////////////////////////////////////////////////////////////////////////////

#ifdef ENABLE_SHADOWS
    uniform sampler2DArray s_csmArray;  // Cascaded Shadow Map array texture
#endif

////////////////////////////////////////////////////////////////////////////////
// IMAGE-BASED LIGHTING (IBL) UNIFORMS
////////////////////////////////////////////////////////////////////////////////
// IBL provides indirect lighting from the environment, split into:
//   - Diffuse: Irradiance map (pre-convolved with cosine lobe)
//   - Specular: Pre-filtered radiance map + DFG LUT (split-sum approximation)
//
// The split-sum approximation separates the IBL integral:
//   L_out ≈ (f0 * DFG.x + f90 * DFG.y) * LD(n, roughness)
//
// Where:
//   DFG = Pre-integrated BRDF response (2D LUT indexed by NdotV and roughness)
//   LD  = Pre-filtered environment map (mip levels store increasing roughness)

#ifdef USE_IMAGE_BASED_LIGHTING
    uniform samplerCube sc_irradiance;  // Diffuse irradiance cubemap (heavily blurred)
    uniform samplerCube sc_radiance;    // Specular radiance cubemap (mip chain for roughness)
    uniform sampler2D s_dfg_lut;        // DFG lookup table for split-sum approximation
#endif

////////////////////////////////////////////////////////////////////////////////
// FRAGMENT SHADER OUTPUT
////////////////////////////////////////////////////////////////////////////////

out vec4 f_color;  // Final fragment color (linear RGB, will be tone-mapped later)

////////////////////////////////////////////////////////////////////////////////
// LIGHTING DATA STRUCTURE
////////////////////////////////////////////////////////////////////////////////
// This structure encapsulates all the precomputed values needed for BRDF evaluation.
// Computing these once per fragment and passing them to lighting functions avoids
// redundant calculations when evaluating multiple lights.

struct LightingInfo
{
    // =========================================================================
    // DOT PRODUCTS (Geometric relationships)
    // =========================================================================
    // These are the fundamental geometric terms used in BRDF equations.
    // All values are clamped to [0,1] to handle backfacing geometry gracefully.

    float NdotL;    // cos(θ_l) = N·L : Angle between surface normal and light direction
                    // Controls diffuse intensity (Lambert's cosine law) and specular visibility.
                    // When NdotL ≤ 0, the surface faces away from the light (in shadow).

    float NdotV;    // cos(θ_v) = N·V : Angle between surface normal and view direction
                    // Used in geometric shadowing and Fresnel calculations.
                    // When NdotV ≤ 0, we're looking at the back of the surface.

    float NdotH;    // cos(θ_h) = N·H : Angle between surface normal and half-vector
                    // Core term for the Normal Distribution Function (NDF).
                    // When NdotH = 1, the microfacet is perfectly aligned for reflection.

    float LdotH;    // cos(θ_d) = L·H : Angle between light direction and half-vector
                    // Used in Fresnel and some visibility approximations.
                    // Note: LdotH = VdotH due to half-vector symmetry.

    float VdotH;    // cos(θ_d) = V·H : Angle between view direction and half-vector
                    // Primary term for Fresnel calculation (angle of incidence on microfacet).
                    // At grazing angles (VdotH → 0), Fresnel reflectance approaches 1.0.

    // =========================================================================
    // ROUGHNESS PARAMETERS
    // =========================================================================

    float perceptualRoughness;
                    // Artist-authored roughness [0,1] as it appears in the material.
                    // This is the "linear" roughness that artists tune.
                    // 0 = perfectly smooth (mirror-like), 1 = fully rough (matte)

    float alphaRoughness;
                    // Material roughness = perceptualRoughness² (Disney/Filament convention)
                    // The squared mapping provides perceptually linear roughness control.
                    // This remapping makes the visual transition more uniform across the range.
                    // Reference: Burley, "Physically-Based Shading at Disney" [2]

    // =========================================================================
    // METALNESS
    // =========================================================================

    float metalness;
                    // Metallic factor [0,1], should be binary in practice (0 or 1).
                    // Metals: No diffuse reflection, specular color = albedo
                    // Dielectrics: Diffuse = albedo, specular color = achromatic F0
                    // Intermediate values are for material transitions (rust on metal, etc.)

    // =========================================================================
    // FRESNEL REFLECTANCE VALUES
    // =========================================================================
    // The Fresnel effect describes how reflectance varies with viewing angle.
    // At normal incidence (looking straight at surface), reflectance = F0.
    // At grazing angles (looking edge-on), reflectance approaches F90 (usually 1.0).

    vec3 reflectance0;
                    // F0: Fresnel reflectance at normal incidence (θ = 0°)
                    // For dielectrics: achromatic, computed as 0.16 * reflectance²
                    // For metals: chromatic, derived from albedo color
                    // Typical values: plastics ~0.04, water ~0.02, metals 0.5-1.0

    vec3 reflectance90;
                    // F90: Fresnel reflectance at grazing angle (θ = 90°)
                    // For most materials this is 1.0 (100% reflection at grazing).
                    // Reduced for very diffuse/low-reflectance materials to prevent
                    // unrealistic brightness at edges.

    // =========================================================================
    // COLOR CONTRIBUTIONS
    // =========================================================================

    vec3 diffuseColor;
                    // Diffuse albedo after metalness adjustment.
                    // diffuseColor = (1 - metallic) * albedo
                    // Metals have no diffuse component (all energy is specular).

    vec3 specularColor;
                    // Specular color for Fresnel base (equals F0/reflectance0).
                    // Used to compute energy compensation and IBL response.
};

////////////////////////////////////////////////////////////////////////////////
// FRESNEL REFLECTANCE (F term of Cook-Torrance BRDF)
////////////////////////////////////////////////////////////////////////////////
//
// THE FRESNEL EFFECT:
// When light hits a surface, some is reflected and some is refracted/absorbed.
// The ratio of reflected light depends on the angle of incidence:
//   - At normal incidence (perpendicular): Reflectance = F0 (material-dependent)
//   - At grazing angles (parallel): Reflectance → F90 (approaches 100% for most materials)
//
// This is why water appears transparent when looking straight down but becomes
// mirror-like when viewed at shallow angles.
//
// SCHLICK'S APPROXIMATION:
// The full Fresnel equations are expensive to compute. Schlick (1994) proposed
// this polynomial approximation that closely matches the physical behavior:
//
//   F_Schlick(θ) = F0 + (F90 - F0) * (1 - cos(θ))^5
//
// Where θ is the angle between view direction and half-vector (VdotH).
// The exponent of 5 was empirically chosen to match the Fresnel curve.
//
// MATHEMATICAL DERIVATION:
// The exact Fresnel equations for unpolarized light are:
//   F = (F_s + F_p) / 2
//   F_s = |n₁cos(θ_i) - n₂cos(θ_t)|² / |n₁cos(θ_i) + n₂cos(θ_t)|²
//   F_p = |n₁cos(θ_t) - n₂cos(θ_t)|² / |n₁cos(θ_t) + n₂cos(θ_i)|²
//
// At normal incidence (θ = 0), this simplifies to:
//   F0 = ((n₁ - n₂) / (n₁ + n₂))²
//
// For air-material interface (n₁ = 1): F0 = ((n - 1) / (n + 1))²
// This is how we compute F0 from Index of Refraction (IOR).
//
// Reference: Schlick, "An Inexpensive BRDF Model for Physically-based Rendering" [4]
//
vec3 specular_reflection(LightingInfo data)
{
    // Schlick's approximation: F(θ) = F0 + (F90 - F0) * (1 - VdotH)^5
    //
    // The clamp ensures numerical stability when VdotH is very close to 1
    // (which would otherwise cause (1-VdotH) to underflow in lower precision).
    //
    // Using VdotH (not NdotV) because Fresnel depends on the angle of incidence
    // onto the microfacet, not the macrosurface. The half-vector H represents
    // the effective microfacet normal for the current L-V configuration.
    return data.reflectance0 + (data.reflectance90 - data.reflectance0) * pow(clamp(1.0 - data.VdotH, 0.0, 1.0), 5.0);
}

////////////////////////////////////////////////////////////////////////////////
// GEOMETRIC SHADOWING/MASKING (G term, expressed as Visibility V)
////////////////////////////////////////////////////////////////////////////////
//
// MICROFACET GEOMETRY:
// At a microscopic level, surfaces are composed of tiny facets. Not all facets
// that are oriented correctly for reflection actually contribute to the output:
//   - SHADOWING: Incoming light is blocked by other microfacets before reaching
//   - MASKING: Outgoing reflected light is blocked before reaching the viewer
//
// The G term models this microfacet self-occlusion. Without it, rough surfaces
// would appear unrealistically bright.
//
// HEIGHT-CORRELATED SMITH MODEL:
// Eric Heitz (2014) showed that treating shadowing and masking as correlated
// (because both depend on the height of microfacets) produces more accurate results.
//
// The Smith model factors G into independent terms per direction:
//   G(v,l,α) = G₁(v,α) * G₁(l,α)     [uncorrelated]
//
// The height-correlated version is:
//   G(v,l,α) = 1 / (1 + Λ(v) + Λ(l))  [correlated]
//
// Where Λ is the Smith lambda function:
//   Λ(m) = (-1 + sqrt(1 + α² * tan²(θ_m))) / 2
//
// VISIBILITY FUNCTION:
// To simplify the BRDF evaluation, we combine G with the denominator:
//   V(v,l,α) = G(v,l,α) / (4 * NdotV * NdotL)
//
// This "visibility function" form allows us to cancel common terms and avoid
// potential division-by-zero issues when NdotV or NdotL approach zero.
//
// MATHEMATICAL DERIVATION (Height-Correlated Smith-GGX):
//   V(v,l,α) = 0.5 / (NdotL * sqrt(NdotV² * (1 - α²) + α²) + NdotV * sqrt(NdotL² * (1 - α²) + α²))
//
// This is the exact formulation used in Filament and recommended by Heitz.
//
// Reference: Heitz, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
//
float visibility_smith_ggx_correlated(float NdotV, float NdotL, float roughness)
{
    // roughness here is alphaRoughness (perceptualRoughness²)
    float a2 = roughness * roughness;  // α⁴ in terms of perceptual roughness

    // GGXV = NdotL * sqrt(NdotV² * (1 - α²) + α²)
    // This is the view-direction shadowing term scaled by NdotL
    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);

    // GGXL = NdotV * sqrt(NdotL² * (1 - α²) + α²)
    // This is the light-direction masking term scaled by NdotV
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);

    // Combined visibility term (includes the 1/4 from BRDF denominator)
    // The 0.5 factor comes from the height-correlated formulation
    return 0.5 / (GGXV + GGXL);
}

// Wrapper function that uses LightingInfo struct for cleaner call sites
float geometric_occlusion(LightingInfo data)
{
    return visibility_smith_ggx_correlated(data.NdotV, data.NdotL, data.alphaRoughness);
}
  
////////////////////////////////////////////////////////////////////////////////
// NORMAL DISTRIBUTION FUNCTION (D term of Cook-Torrance BRDF)
////////////////////////////////////////////////////////////////////////////////
//
// MICROFACET THEORY:
// The NDF describes the statistical distribution of microfacet orientations on
// a surface. It answers: "What proportion of microfacets are aligned with the
// half-vector H?" Only these aligned facets contribute to specular reflection.
//
// GGX / TROWBRIDGE-REITZ DISTRIBUTION:
// This distribution has become the industry standard for PBR due to:
//   1. Long tail falloff (realistic for real materials)
//   2. Short peak in highlights
//   3. Simple mathematical formulation
//   4. Good energy conservation properties
//
// The GGX NDF is defined as:
//
//   D_GGX(h,α) = α² / (π * ((N·H)² * (α² - 1) + 1)²)
//
// Properties:
//   - When α → 0: D approaches a Dirac delta (perfect mirror, infinitely sharp)
//   - When α → 1: D becomes broad and flat (very diffuse specular)
//   - D is normalized: ∫ D(h,α) * (N·H) dω = 1
//
// ROUGHNESS REMAPPING:
// The 'a' parameter is alphaRoughness = perceptualRoughness²
// The 'roughnessSq' below is α² = perceptualRoughness⁴
// This double-square remapping ensures perceptually linear roughness control.
//
// NUMERICAL FORM DERIVATION:
// Starting from: D = α² / (π * ((NdotH)² * (α² - 1) + 1)²)
// Let f = (NdotH * α² - NdotH) * NdotH + 1
//       = NdotH² * α² - NdotH² + 1
//       = NdotH² * (α² - 1) + 1
// Then: D = α² / (π * f²)
//
// This algebraic rearrangement avoids a separate NdotH² calculation.
//
// Reference: Trowbridge & Reitz, "Average Irregularity Representation of a Roughened Surface" (1975)
// Reference: Walter et al., "Microfacet Models for Refraction through Rough Surfaces" (2007)
//
float microfacet_distribution(LightingInfo data)
{
    // roughnessSq = α² = (perceptualRoughness²)² = perceptualRoughness⁴
    float roughnessSq = data.alphaRoughness * data.alphaRoughness;

    // Algebraically equivalent to: (NdotH² * (α² - 1) + 1)
    // This form: (NdotH * α² - NdotH) * NdotH + 1
    // Factors out NdotH to reduce operations
    float f = (data.NdotH * roughnessSq - data.NdotH) * data.NdotH + 1.0;

    // D = α² / (π * f²)
    return roughnessSq / (PI * f * f);
}

////////////////////////////////////////////////////////////////////////////////
// GGX DISTRIBUTION (Direct parameterization for clear coat)
////////////////////////////////////////////////////////////////////////////////
// This version takes alpha (roughness²) directly rather than using LightingInfo.
// Used for the clear coat layer which has its own roughness parameter.
//
// HALF-PRECISION OPTIMIZATION
// The standard formulation computes 1 - NdotH² which suffers from floating-point
// cancellation when NdotH is close to 1 (specular highlights). This is problematic
// for half-precision floats (fp16) on mobile GPUs.
//
// Using Lagrange's identity: |a × b|² = |a|²|b|² - (a·b)²
// Since n and h are unit vectors: |n × h|² = 1 - (n·h)²
//
// This allows computing 1 - NdotH² via cross product, which is numerically
// stable in half-precision.
//
float D_GGX(float NdotH, float a, vec3 n, vec3 h)
{
    // Cross-product trick for half-precision stability
    vec3 NxH = cross(n, h);
    float oneMinusNdotHSquared = dot(NxH, NxH);  // = 1 - NdotH²

    float a2 = a * a;
    // Reformulated: D = a² / (π * (a² + (1-NdotH²)/NdotH² * (1-a²))² * NdotH⁴)
    // Simplified: use the identity with cross product
    float denom = oneMinusNdotHSquared + NdotH * NdotH * a2;
    return a2 / (PI * denom * denom);
}

// Legacy version without half-precision optimization (for compatibility)
float D_GGX(float NdotH, float a)
{
    float a2 = a * a;  // α² (where a is already perceptualRoughness²)
    float f = (NdotH * a2 - NdotH) * NdotH + 1.0;
    return a2 / (PI * f * f);
}

////////////////////////////////////////////////////////////////////////////////
// SPECULAR OCCLUSION
////////////////////////////////////////////////////////////////////////////////
//
// Standard ambient occlusion is designed for diffuse lighting and doesn't
// correctly handle specular reflections. Specular light comes from specific
// directions (reflection vector), not uniformly from the hemisphere.
//
// Lagarde's specular occlusion approximation accounts for this by considering
// that specular occlusion depends on both the AO value and the view angle:
//   - At grazing angles, specular sees more of the hemisphere (less occlusion)
//   - At normal incidence, specular is more focused (more affected by AO)
//
// Formula: specularAO = saturate(pow(NdotV + ao, exp) - 1 + ao)
//
// The exponent controls the falloff; higher values = sharper transition.
// A value of 2.0 works well for most cases.
//
// Reference: Lagarde, "Adopting a physically based shading model" (Frostbite)
//
float compute_specular_occlusion(float NdotV, float ao, float roughness)
{
    // The exponent is modulated by roughness: rougher surfaces need less
    // specular occlusion because their specular lobe is wider
    float exp_val = exp2(-16.0 * roughness - 1.0);
    return clamp(pow(NdotV + ao, exp_val) - 1.0 + ao, 0.0, 1.0);
}

////////////////////////////////////////////////////////////////////////////////
// GEOMETRIC SPECULAR ANTI-ALIASING
////////////////////////////////////////////////////////////////////////////////
//
// Normal maps cause specular aliasing because high-frequency normal variations
// create sub-pixel specular highlights that flicker as the camera moves.
//
// TWO COMPLEMENTARY APPROACHES:
//
// 1. TOKSVIG METHOD (mipmap-based):
//    When normal maps are filtered (mipmapped), averaged normals become shorter.
//    A normal of length 0.5 indicates high variance in the source normals.
//    We increase roughness proportionally to blur the specular highlight.
//
//    Formula: σ² = 1 - |n_filtered|
//    Adjusted α = sqrt(α² + k * σ²)
//
//    REQUIREMENT: Normal maps must have proper mipmaps with averaged (not
//    renormalized) normals. Many asset pipelines normalize mipmap normals,
//    which defeats this method.
//
// 2. SCREEN-SPACE DERIVATIVES (Kaplanyan method):
//    Use dFdx/dFdy to measure how quickly the normal changes across the screen.
//    High derivatives = high-frequency detail = needs roughness increase.
//
//    This method works regardless of mipmap quality and catches geometric
//    aliasing from mesh tessellation as well as normal maps.
//
//    Formula: σ² = max(|dN/dx|², |dN/dy|²)
//
// We combine both methods for robust anti-aliasing.
//
// Reference: Toksvig, "Mipmapping Normal Maps" (2004)
// Reference: Kaplanyan, "Next Generation Post Processing in Call of Duty" (2014)
// Reference: Filament docs, "Geometric Specular Anti-Aliasing"
//

// Toksvig method: estimate variance from filtered normal length
// IMPORTANT: normal_sample must be the RAW sample, NOT normalized!
float compute_toksvig_roughness(float roughness, vec3 normal_sample_raw, float strength)
{
    float normal_len = length(normal_sample_raw);

    // Variance from normal length (shorter = more variance from averaging)
    // Clamp to handle edge cases where normal_len > 1 due to filtering artifacts
    float variance = clamp(1.0 - normal_len, 0.0, 1.0);

    // Square the variance for a more gradual response curve
    variance = variance * variance;

    // Combine with existing roughness using the Toksvig formula
    float alpha = roughness * roughness;
    float adjusted_alpha = sqrt(alpha * alpha + strength * variance);

    return sqrt(adjusted_alpha);
}

// Screen-space derivative method: estimate variance from normal rate of change
// This catches aliasing that Toksvig misses (bad mipmaps, geometric detail)
float compute_screen_space_roughness(float roughness, vec3 world_normal, float strength)
{
    // Compute screen-space derivatives of the world normal
    // These measure how quickly the normal changes per pixel
    vec3 dNdx = dFdx(world_normal);
    vec3 dNdy = dFdy(world_normal);

    // Variance estimate: magnitude of the rate of change
    // Using squared length avoids sqrt, and max handles anisotropic cases
    float variance = max(dot(dNdx, dNdx), dot(dNdy, dNdy));

    // The variance from derivatives can be quite large, so we apply a
    // perceptual curve to keep the roughness adjustment reasonable
    // This prevents overly aggressive blurring while still catching aliasing
    variance = min(variance * strength, 0.5);  // Cap at 0.5 to preserve material look

    // Combine with existing roughness
    float alpha = roughness * roughness;
    float adjusted_alpha = sqrt(alpha * alpha + variance);

    return sqrt(adjusted_alpha);
}

// Combined geometric AA: applies both Toksvig and screen-space methods
float compute_geometric_aa_roughness(float roughness, vec3 normal_sample_raw, vec3 world_normal, float toksvig_strength, float screenspace_strength)
{
    // Apply Toksvig (mipmap-based) - effective when normal maps have proper mipmaps
    float r = compute_toksvig_roughness(roughness, normal_sample_raw, toksvig_strength);

    // Apply screen-space derivatives - catches remaining aliasing
    r = compute_screen_space_roughness(r, world_normal, screenspace_strength);

    return r;
}

////////////////////////////////////////////////////////////////////////////////
// KELEMEN VISIBILITY FUNCTION (for Clear Coat layer)
////////////////////////////////////////////////////////////////////////////////
//
// For the clear coat layer, we use a cheaper visibility function since:
//   1. Clear coat is typically very smooth (low roughness)
//   2. Visual difference from Smith-GGX is minimal at low roughness
//   3. Significant performance savings
//
// KELEMEN APPROXIMATION:
//   V_Kelemen(l,h) = 1 / (4 * (L·H)²)
//
// Note: The 0.25 = 1/4 factor is part of the visibility formulation.
// This is NOT height-correlated but is physically plausible and very cheap.
//
// This approximation works well because:
//   - At low roughness, the masking/shadowing effects are minimal
//   - The half-vector angle (LdotH) captures the essential geometry
//   - Avoids expensive sqrt operations of Smith-GGX
//
// CAVEAT: This is not physically based per Heitz's analysis, but the error
// is acceptable for the thin, smooth clear coat layer in practice.
//
// Reference: Kelemen & Szirmay-Kalos, "A Microfacet Based Coupled Specular-Matte BRDF Model" (2001)
// Reference: Filament documentation, "Clear coat model" section
//
float visibility_kelemen(float LdotH)
{
    // V = 0.25 / (LdotH)²
    // The 0.25 factor includes the 1/4 from the BRDF denominator simplification
    return 0.25 / (LdotH * LdotH);
}

////////////////////////////////////////////////////////////////////////////////
// COOK-TORRANCE BRDF EVALUATION
////////////////////////////////////////////////////////////////////////////////
//
// This function evaluates the complete Cook-Torrance microfacet BRDF for a single
// light source, outputting separate diffuse and specular contributions.
//
// THE COOK-TORRANCE MODEL:
// The surface response is split into diffuse and specular components:
//   f(v,l) = f_d(v,l) + f_r(v,l)
//
// DIFFUSE COMPONENT (Lambertian):
//   f_d = (1 - F) * (diffuseColor / π)
//
//   The (1 - F) factor represents energy conservation: light that is reflected
//   specularly (F) is NOT available for diffuse scattering. This Fresnel
//   weighting ensures energy balance at all viewing angles.
//
//   The π divisor comes from the Lambertian BRDF normalization:
//   ∫ (σ/π) * cos(θ) dω = σ (over hemisphere)
//
// SPECULAR COMPONENT (Cook-Torrance):
//   f_r = D * F * G / (4 * NdotV * NdotL)
//
//   Simplified using visibility function V = G / (4 * NdotV * NdotL):
//   f_r = D * F * V
//
// PARAMETERS:
//   data        - Precomputed lighting geometry and material properties
//   attenuation - Light intensity/falloff multiplier
//
// OUTPUTS:
//   diffuseContribution  - Diffuse BRDF response × attenuation
//   specularContribution - Specular BRDF response × attenuation
//
// Note: The NdotL term (Lambert's cosine law) is applied in the main() function,
// not here. This allows for easier shadow integration.
//
void compute_cook_torrance(LightingInfo data, float attenuation, out vec3 diffuseContribution, out vec3 specularContribution)
{
    // =========================================================================
    // EVALUATE BRDF TERMS
    // =========================================================================

    // F: Fresnel reflectance - how much light is specularly reflected vs transmitted
    // Uses Schlick approximation: F0 + (F90 - F0) * (1 - VdotH)^5
    const vec3 F = specular_reflection(data);

    // V: Visibility/Geometric shadowing - accounts for microfacet self-occlusion
    // Uses height-correlated Smith-GGX (includes 1/4 factor)
    const float V = geometric_occlusion(data);

    // D: Normal Distribution Function - proportion of correctly oriented microfacets
    // Uses GGX/Trowbridge-Reitz distribution
    const float D = microfacet_distribution(data);

    // =========================================================================
    // DIFFUSE CONTRIBUTION
    // =========================================================================
    // Lambertian diffuse: f_d = diffuseColor / π
    // Energy conservation: multiply by (1 - F) since specular light isn't diffused
    //
    // Note: For metals (metalness = 1), diffuseColor = 0, so this naturally
    // evaluates to zero, correctly modeling that metals have no diffuse component.

    diffuseContribution = ((1.0 - F) * (data.diffuseColor / PI)) * attenuation;

    // =========================================================================
    // SPECULAR CONTRIBUTION
    // =========================================================================
    // Cook-Torrance specular: f_r = D * V * F
    // (V already incorporates the G / (4 * NdotV * NdotL) simplification)

    specularContribution = (F * V * D) * attenuation;
}

////////////////////////////////////////////////////////////////////////////////
// MAIN FRAGMENT SHADER ENTRY POINT
////////////////////////////////////////////////////////////////////////////////
void main()
{
    // =========================================================================
    // SECTION 1: MATERIAL PROPERTY SAMPLING
    // =========================================================================
    // Gather base material properties from uniforms and texture maps.
    // Texture samples modulate uniform values, allowing for detail variation.

    // Base albedo color from uniform (linear RGB)
    vec3 albedo = u_albedo;

    // Normalize interpolated normal (interpolation can denormalize)
    vec3 N = normalize(v_normal);

    // -------------------------------------------------------------------------
    // ROUGHNESS CLAMPING:
    // Minimum roughness is 0.089 (not 0!) for several important reasons:
    //
    // 1. NUMERICAL STABILITY: The GGX NDF has α⁴ in the denominator.
    //    For half-precision floats (fp16), the minimum representable value
    //    is 2^-14 ≈ 6.1×10^-5. With roughness = 0.089:
    //    α⁴ = 0.089⁴ ≈ 6.3×10^-5, safely above the fp16 minimum.
    //
    // 2. SPECULAR ALIASING: Extremely sharp specular highlights (roughness → 0)
    //    cause temporal flickering and aliasing artifacts. The minimum roughness
    //    provides a "natural" anti-aliasing effect.
    //
    // 3. DIVISION BY ZERO: Some BRDF terms have roughness in denominators.
    //
    // Reference: Filament docs, "Roughness remapping and clamping" section
    // -------------------------------------------------------------------------
    float roughness = clamp(u_roughness, 0.089, 1.0);
    float metallic = u_metallic;

    // -------------------------------------------------------------------------
    // NORMAL MAPPING:
    // Transform tangent-space normal map sample to world space.
    // The normal map stores normals as RGB where (0.5, 0.5, 1.0) = flat surface.
    // Decoding: normalTS = sample * 2.0 - 1.0 maps [0,1] → [-1,1]
    // -------------------------------------------------------------------------
#ifdef HAS_NORMAL_MAP
    // Sample normal map and decode from [0,1] to [-1,1] range
    // IMPORTANT: Keep the raw sample for Toksvig geometric AA (before normalizing)
    // The raw length indicates normal map variance from mipmap filtering
    vec3 nSampleRaw = texture(s_normal, v_texcoord).xyz * 2.0 - vec3(1.0);
    vec3 nSample = normalize(nSampleRaw);
    // Transform from tangent space to world space using TBN matrix
    // calc_normal_map constructs TBN from interpolated tangent frame
    N = normalize(calc_normal_map(v_normal, normalize(v_tangent), normalize(v_bitangent), nSample).xyz);
#endif

    // -------------------------------------------------------------------------
    // ROUGHNESS MAP:
    // Typically stored in green channel of combined ORM (Occlusion/Roughness/Metallic) texture.
    // The map value multiplies the base roughness uniform.
    // -------------------------------------------------------------------------
#ifdef HAS_ROUGHNESS_MAP
    roughness = texture(s_roughness, v_texcoord).r * roughness;
#endif

    // -------------------------------------------------------------------------
    // METALNESS MAP:
    // Typically stored in blue channel of combined ORM texture.
    // The map value multiplies the base metallic uniform.
    // -------------------------------------------------------------------------
#ifdef HAS_METALNESS_MAP
    metallic = texture(s_metallic, v_texcoord).r * metallic;
#endif

    // -------------------------------------------------------------------------
    // ALBEDO/BASE COLOR MAP:
    // Stored in sRGB color space in texture, must convert to linear for lighting.
    // The map value multiplies the base albedo uniform.
    // -------------------------------------------------------------------------
#ifdef HAS_ALBEDO_MAP
    albedo *= sRGBToLinear(texture(s_albedo, v_texcoord).rgb, DEFAULT_GAMMA);
#endif

    // -------------------------------------------------------------------------
    // GEOMETRIC SPECULAR ANTI-ALIASING
    // -------------------------------------------------------------------------
    // Two complementary methods are used to reduce specular aliasing:
    //
    // 1. TOKSVIG METHOD (mipmap-based):
    //    Uses the length of the raw (non-normalized) normal sample to estimate
    //    variance from mipmap filtering. Shorter normals = more variance = higher
    //    roughness. Requires proper mipmap generation that averages normals
    //    without renormalizing.
    //
    // 2. SCREEN-SPACE DERIVATIVES (Kaplanyan method):
    //    Uses dFdx/dFdy to measure how quickly the normal changes per pixel.
    //    High derivatives indicate high-frequency detail that should be blurred.
    //    This method works regardless of mipmap quality.
    //
    // Strength parameters control how aggressively each method adjusts roughness:
    //   - toksvig_strength (0.75): Multiplier for mipmap-based variance
    //   - screenspace_strength (2.0): Multiplier for derivative-based variance
    //
    // Reference: Toksvig, "Mipmapping Normal Maps" (2004)
    // Reference: Kaplanyan, "Next Generation Post Processing in Call of Duty" (2014)
    // -------------------------------------------------------------------------
#ifdef HAS_NORMAL_MAP
    roughness = compute_geometric_aa_roughness(roughness, nSampleRaw, N, 0.75, 3.0);
    // Re-clamp after adjustment to maintain valid range
    roughness = clamp(roughness, 0.089, 1.0);
#endif

    // Ensure metallic stays in valid range after texture modulation
    metallic = clamp(metallic, 0.0, 1.0);

    // =========================================================================
    // SECTION 2: ROUGHNESS REMAPPING
    // =========================================================================
    // Convert perceptual roughness to material roughness (alpha).
    //
    // DISNEY/FILAMENT CONVENTION:
    //   alphaRoughness = perceptualRoughness²
    //
    // This squared remapping makes roughness control perceptually linear:
    // equal increments in the slider produce visually equal changes in
    // highlight sharpness. Without remapping, the 0-0.1 range would contain
    // most of the interesting variation.
    //
    // Reference: Burley, "Physically-Based Shading at Disney" [2]
    //
    // @todo: Some implementations use α² (perceptualRoughness⁴) in the NDF.
    // The current implementation uses perceptualRoughness² as alpha, then
    // squares again in the NDF, effectively using perceptualRoughness⁴.
    // -------------------------------------------------------------------------
    const float alphaRoughness = roughness * roughness;

    // =========================================================================
    // SECTION 3: VIEW VECTOR AND GEOMETRIC SETUP
    // =========================================================================

    // View vector: direction from surface point to camera (world space)
    vec3 V = normalize(u_eyePos.xyz - v_world_position);

    // -------------------------------------------------------------------------
    // NdotV CALCULATION WITH BIAS:
    // Using abs() instead of max() to handle backfacing fragments gracefully.
    // The 0.001 bias prevents division-by-zero in the visibility function
    // and reduces artifacts at grazing angles.
    //
    // Note: Some implementations use max(dot(N,V), 1e-5) instead.
    // The abs() approach better handles double-sided materials.
    // -------------------------------------------------------------------------
    float NdotV = abs(dot(N, V)) + 0.001;

    // =========================================================================
    // SECTION 4: FRESNEL F0 CALCULATION
    // =========================================================================
    //
    // F0 (Fresnel reflectance at normal incidence) determines the base
    // reflectivity of a material. The calculation differs for metals vs dielectrics:
    //
    // DIELECTRICS (metallic = 0):
    //   F0 = 0.16 * reflectance²
    //
    //   This remapping was chosen to provide an intuitive artist parameter:
    //   - reflectance = 0.35 → F0 = 0.02 (2%, water)
    //   - reflectance = 0.5  → F0 = 0.04 (4%, most plastics/glass) [DEFAULT]
    //   - reflectance = 1.0  → F0 = 0.16 (16%, gemstones)
    //
    //   The 0.16 constant = 0.04 / 0.5² ensures reflectance=0.5 gives 4%.
    //
    // METALS (metallic = 1):
    //   F0 = albedo
    //
    //   Metals have no diffuse component; their color comes entirely from
    //   specular reflection. The albedo IS the specular color (F0).
    //   Typical metal F0 values: 0.5-1.0 (chromatic).
    //
    // MIXED (0 < metallic < 1):
    //   Linear interpolation between dielectric and metallic F0.
    //   Used for material transitions (e.g., rust spots on metal).
    //
    // Reference: Filament docs, "Reflectance remapping" section
    // -------------------------------------------------------------------------
    vec3 F0 = 0.16 * u_reflectance * u_reflectance * (1.0 - metallic) + albedo * metallic;

    // =========================================================================
    // SECTION 5: DIFFUSE COLOR CALCULATION
    // =========================================================================
    //
    // DIELECTRIC-METAL ENERGY SPLIT:
    // - Dielectrics: Light penetrates, scatters internally, exits as diffuse
    // - Metals: No subsurface scattering; all reflection is specular
    //
    // diffuseColor = (1 - metallic) * albedo
    //
    // For metallic = 1: diffuseColor = 0 (no diffuse from metals)
    // For metallic = 0: diffuseColor = albedo (full diffuse from dielectrics)
    //
    // Note: The additional (1-F) energy conservation term is applied
    // dynamically in the BRDF, not baked into diffuseColor here.
    // -------------------------------------------------------------------------
    vec3 diffuseColor = (1.0 - metallic) * albedo;

    // =========================================================================
    // SECTION 6: F90 (GRAZING REFLECTANCE) CALCULATION
    // =========================================================================
    //
    // F90 is the Fresnel reflectance at grazing angles (90° from normal).
    // For most materials, F90 ≈ 1.0 (100% reflection at grazing angles).
    //
    // SPECULAR OCCLUSION CONSIDERATION:
    // For very dark/diffuse materials with F0 < 4%, using F90 = 1.0 would
    // create unrealistic bright edges. The formula below gradually reduces
    // F90 for low-reflectance materials:
    //
    //   reflectanceLuminance = max(F0.r, F0.g, F0.b)
    //   F90 = clamp(reflectanceLuminance * 25, 0, 1)
    //
    // Examples:
    //   - F0 = 0.04 (typical) → F90 = clamp(1.0) = 1.0 ✓
    //   - F0 = 0.02 (low)     → F90 = clamp(0.5) = 0.5
    //   - F0 = 0.01 (very low)→ F90 = clamp(0.25)= 0.25
    //
    // The multiplier 25 was chosen so that F0 ≥ 0.04 yields F90 = 1.0.
    // -------------------------------------------------------------------------
    vec3 specularColor = F0;
    float reflectanceLuminance = max(max(specularColor.r, specularColor.g), specularColor.b);

    // F90 calculation with specular occlusion for low-reflectance materials
    float reflectance90 = clamp(reflectanceLuminance * 25, 0.0, 1.0);

    // Package F0 and F90 for use in BRDF calculations
    vec3 specularEnvironmentR0 = specularColor.rgb;                     // F0
    vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;  // F90

    // =========================================================================
    // SECTION 7: LIGHTING ACCUMULATION SETUP
    // =========================================================================
    //
    // Separate accumulation of lighting contributions allows proper handling of:
    //   - Direct lighting (affected by shadows)
    //   - Indirect/IBL lighting (NOT affected by shadows, only AO)
    //   - Emissive (NOT affected by shadows or AO)
    //
    // For clear coat energy conservation, we need to track diffuse
    // and specular separately because:
    //   - Diffuse is attenuated by (1 - Fc) once
    //   - Specular is attenuated by (1 - Fc)² (light passes through clear coat twice)
    //
    // Final combination with clear coat:
    //   direct_diffuse * (1-Fc) + direct_specular * (1-Fc)² + clearCoat
    // -------------------------------------------------------------------------
    vec3 Lo_direct_diffuse = vec3(0, 0, 0);   // Accumulated direct diffuse
    vec3 Lo_direct_specular = vec3(0, 0, 0);  // Accumulated direct specular
    vec3 Lo_indirect_diffuse = vec3(0, 0, 0); // Accumulated IBL diffuse
    vec3 Lo_indirect_specular = vec3(0, 0, 0);// Accumulated IBL specular
    vec3 Lo_emissive = vec3(0, 0, 0);         // Accumulated emissive contribution

    // Shadow calculation state
    vec3 debugShadowColor;       // Debug visualization for CSM cascade selection
    float shadowVisibility = 1.0; // 1 = fully lit, 0 = fully shadowed

    // =========================================================================
    // SECTION 8: DIRECTIONAL LIGHT EVALUATION
    // =========================================================================
    //
    // Directional lights (sun, moon) are infinitely distant, so:
    //   - All rays are parallel (L is constant for all fragments)
    //   - No distance attenuation
    //   - Usually casts shadows via Cascaded Shadow Maps (CSM)
    //
    // THE RENDERING EQUATION for a single light:
    //   Lo = ∫ f(v,l) * Li * (N·L) dω
    //
    // For a directional light (point source on sphere at infinity):
    //   Lo = f(v,L) * Li * max(N·L, 0)
    //
    // Where:
    //   f(v,L) = BRDF evaluation (diffuse + specular)
    //   Li     = Incident radiance (light color × intensity)
    //   N·L    = Lambert's cosine law (geometric attenuation)
    // -------------------------------------------------------------------------
    if (sunlightActive > 0)
    {
        // Light direction (pointing FROM surface TO light)
        vec3 L = normalize(u_directionalLight.direction);

        // Half-vector: bisector between view and light directions
        // For perfect mirrors (roughness=0), N=H means perfect reflection
        vec3 H = normalize(L + V);

        // ---------------------------------------------------------------------
        // GEOMETRIC DOT PRODUCTS:
        // These are the fundamental building blocks of the BRDF.
        // All clamped to prevent negative values from backfacing geometry.
        //
        // NdotL: Surface facing light? Controls overall contribution.
        //        Clamped to 0.001 to avoid artifacts at light terminator.
        // NdotH: Microfacet alignment. Higher = sharper highlights.
        // LdotH: Light angle to half-vector. Used in Fresnel.
        // VdotH: View angle to half-vector. Equals LdotH by symmetry.
        // ---------------------------------------------------------------------
        float NdotL = clamp(dot(N, L), 0.001, 1.0);
        float NdotH = clamp(dot(N, H), 0.0, 1.0);
        float LdotH = clamp(dot(L, H), 0.0, 1.0);
        float VdotH = clamp(dot(V, H), 0.0, 1.0);

        // Package all parameters for BRDF evaluation
        LightingInfo data = LightingInfo(
            NdotL, NdotV, NdotH, LdotH, VdotH,
            roughness, alphaRoughness, metallic, specularEnvironmentR0, specularEnvironmentR90,
            diffuseColor, specularColor
        );

        // ---------------------------------------------------------------------
        // SHADOW MAPPING (Cascaded Shadow Maps)
        // ---------------------------------------------------------------------
        // CSM splits the view frustum into cascades, each with its own shadow map.
        // This provides high-resolution shadows near the camera and acceptable
        // resolution for distant shadows.
        //
        // SHADOW BIASING:
        // Shadow acne (self-shadowing artifacts) is combated with two bias types:
        //   - slope_bias: Scales with surface slope relative to light
        //   - normal_bias: Pushes sample point along surface normal
        //
        // The biased_pos is used for shadow map lookup to prevent self-occlusion.
        // ---------------------------------------------------------------------
        float NdotL_S = clamp(dot(v_normal, L), 0.001, 1.0);  // Using unperturbed normal for bias
        const float slope_bias = 0.04;   // Slope-dependent bias factor
        const float normal_bias = 0.01;  // Normal-offset bias factor
        vec3 biased_pos = get_biased_position(v_world_position, slope_bias, normal_bias, v_normal, L);

        float shadowTerm = 1.0;  // 1 = not in shadow
        #ifdef ENABLE_SHADOWS
            // Calculate shadow factor from CSM (0 = in shadow, 1 = lit)
            shadowTerm = calculate_csm_coefficient(s_csmArray, biased_pos, v_view_space_position, u_cascadesMatrix, u_cascadesPlane, debugShadowColor);
            // Combine shadow with NdotL to fade shadows at light terminator
            // u_shadowOpacity controls shadow darkness, u_receiveShadow is per-object flag
            shadowVisibility = 1.0 - ((shadowTerm * NdotL) * u_shadowOpacity * u_receiveShadow);
        #endif

        // ---------------------------------------------------------------------
        // BRDF EVALUATION
        // ---------------------------------------------------------------------
        vec3 diffuseContrib, specContrib;
        compute_cook_torrance(data, u_directionalLight.amount, diffuseContrib, specContrib);

        // Apply Lambert's cosine law (NdotL) and light color
        // BRDF × Light_color × NdotL × intensity
        // Accumulate diffuse and specular separately for clear coat attenuation
        vec3 lightContrib = NdotL * u_directionalLight.color;
        Lo_direct_diffuse += lightContrib * diffuseContrib;
        Lo_direct_specular += lightContrib * specContrib;
    }

    // =========================================================================
    // SECTION 9: POINT LIGHT EVALUATION
    // =========================================================================
    //
    // Point lights are infinitesimally small light sources that emit uniformly
    // in all directions. Key differences from directional lights:
    //   - L varies per fragment (computed from light position)
    //   - Distance attenuation follows inverse-square law
    //   - Finite influence radius for culling/performance
    //
    // INVERSE-SQUARE LAW:
    // Physically, light intensity decreases with the square of distance:
    //   E = I / d²
    //
    // However, this is impractical for real-time rendering because:
    //   1. Division by zero when d → 0 (light inside geometry)
    //   2. Infinite influence range (every light affects every pixel)
    //
    // The point_light_attenuation function handles these with:
    //   - Minimum distance clamp (treats light as small sphere)
    //   - Smooth falloff to zero at influence radius
    //
    // Reference: Karis, "Real Shading in Unreal Engine 4" [1]
    // -------------------------------------------------------------------------
    for (int i = 0; i < u_activePointLights; ++i)
    {
        // Light direction: from surface to light position (world space)
        vec3 L = normalize(u_pointLights[i].position - v_world_position);

        // Half-vector for this light-view configuration
        vec3 H = normalize(L + V);

        // ---------------------------------------------------------------------
        // GEOMETRIC DOT PRODUCTS (same meaning as directional light)
        // ---------------------------------------------------------------------
        float NdotL = clamp(dot(N, L), 0.001, 1.0);
        float NdotH = clamp(dot(N, H), 0.0, 1.0);
        float LdotH = clamp(dot(L, H), 0.0, 1.0);
        float VdotH = clamp(dot(V, H), 0.0, 1.0);

        // Package parameters for BRDF evaluation
        LightingInfo data = LightingInfo(
            NdotL, NdotV, NdotH, LdotH, VdotH,
            roughness, alphaRoughness, metallic, specularEnvironmentR0, specularEnvironmentR90,
            diffuseColor, specularColor
        );

        // ---------------------------------------------------------------------
        // DISTANCE ATTENUATION
        // ---------------------------------------------------------------------
        // Calculate distance from surface to light
        float dist = length(u_pointLights[i].position - v_world_position);

        // point_light_attenuation parameters:
        //   - radius: Light's influence radius (falloff reaches zero here)
        //   - 2.0: Attenuation exponent (2 = physically correct inverse-square)
        //   - 0.1: Minimum distance clamp (prevents divide-by-zero)
        //   - dist: Actual distance to light
        //
        // Returns smoothly attenuated factor [0, 1]
        float attenuation = point_light_attenuation(u_pointLights[i].radius, 2.0, 0.1, dist);

        // ---------------------------------------------------------------------
        // BRDF EVALUATION
        // ---------------------------------------------------------------------
        vec3 diffuseContrib, specContrib;
        compute_cook_torrance(data, attenuation, diffuseContrib, specContrib);

        // Final contribution: BRDF × Light_color × NdotL
        // Note: attenuation is already applied inside compute_cook_torrance
        // Accumulate diffuse and specular separately for clear coat attenuation
        vec3 lightContrib = NdotL * u_pointLights[i].color;
        Lo_direct_diffuse += lightContrib * diffuseContrib;
        Lo_direct_specular += lightContrib * specContrib;
    }

    // =========================================================================
    // SECTION 10: IMAGE-BASED LIGHTING (IBL)
    // =========================================================================
    //
    // IBL simulates indirect lighting from the environment by using cubemap
    // textures that encode lighting information from all directions.
    //
    // THE IBL INTEGRAL:
    //   Lo(n,v) = ∫Ω f(l,v) × Li(l) × (N·L) dl
    //
    // This integral is too expensive to compute at runtime, so we use the
    // SPLIT-SUM APPROXIMATION (Epic/Karis):
    //
    //   Lo ≈ Li(r) × (F0 × DFG.x + F90 × DFG.y)
    //
    // Where:
    //   Li(r) = Pre-filtered environment (LD term, stored in mip chain)
    //   DFG   = Pre-integrated BRDF response (2D LUT)
    //
    // TWO COMPONENTS:
    //   1. DIFFUSE IBL: Irradiance map (Lambertian pre-integration)
    //   2. SPECULAR IBL: Pre-filtered radiance + DFG LUT
    //
    // IMPORTANT: IBL is indirect lighting and should NOT be affected by
    // direct light shadows. Only ambient occlusion should modulate IBL.
    // -------------------------------------------------------------------------
    #ifdef USE_IMAGE_BASED_LIGHTING
    {
        // The pre-filtered radiance cubemap stores increasing roughness levels
        // in successive mip levels. This allows us to sample the appropriate
        // "blur" of the environment based on material roughness.
        //
        // NUM_MIP_LEVELS corresponds to the number of mip levels in the cubemap.
        // For a 256×256 cubemap: 9 levels (256, 128, 64, 32, 16, 8, 4, 2, 1)
        // For a 128×128 cubemap: 8 levels
        // For a 64×64 cubemap: 7 levels
        //
        // Use perceptualRoughness directly as the LOD coordinate for linear
        // roughness response. The pre-filtered cubemap was generated with
        // roughness stored at each mip level using:
        //   lod_alpha = alpha^(1/2) = perceptualRoughness
        //
        // This gives perceptually linear roughness transitions in reflections.
        //
        // Formula: mipLevel = perceptualRoughness * (NUM_MIP_LEVELS - 1)
        // ---------------------------------------------------------------------
        const int NUM_MIP_LEVELS = 6;
        float mipLevel = roughness * float(NUM_MIP_LEVELS - 1);  // roughness IS perceptualRoughness here

        // ---------------------------------------------------------------------
        // REFLECTION VECTOR AND CUBEMAP LOOKUP
        // ---------------------------------------------------------------------
        // The reflection vector determines where to sample the specular cubemap.
        // R = reflect(-V, N) = 2(N·V)N - V
        //
        // fix_cube_lookup corrects for cubemap seam issues at low mip levels
        // by biasing the lookup direction slightly inward.
        // ---------------------------------------------------------------------
        vec3 R = reflect(-V, N);
        vec3 cubemapLookup = fix_cube_lookup(R, 512, mipLevel);

        // ---------------------------------------------------------------------
        // DIFFUSE IRRADIANCE SAMPLING
        // ---------------------------------------------------------------------
        // The irradiance map is pre-convolved with a cosine lobe, encoding
        // the integral: ∫Ω Li(l) × (N·L) dl for each normal direction.
        //
        // Sample direction: N (surface normal)
        // This gives us the total diffuse light arriving at the surface.
        //
        // sRGBToLinear: Cubemaps are typically stored in sRGB for precision.
        // ---------------------------------------------------------------------
        vec3 irradiance = sRGBToLinear(texture(sc_irradiance, N).rgb, DEFAULT_GAMMA) * u_ambientStrength;

        // ---------------------------------------------------------------------
        // SPECULAR RADIANCE SAMPLING (LD term)
        // ---------------------------------------------------------------------
        // Sample the pre-filtered environment at the appropriate roughness level.
        // Higher mip = more blur = rougher surface appearance.
        // ---------------------------------------------------------------------
        vec3 radiance = sRGBToLinear(textureLod(sc_radiance, cubemapLookup, mipLevel).rgb, DEFAULT_GAMMA) * u_ambientStrength;

        // ---------------------------------------------------------------------
        // DFG LUT LOOKUP (Split-Sum BRDF Pre-Integration)
        // ---------------------------------------------------------------------
        // The DFG LUT encodes the pre-integrated BRDF response:
        //   dfg.x = ∫ (1 - Fc) × V × (VdotH/NdotH) × NdotL ... (scales F0)
        //   dfg.y = ∫ Fc × V × (VdotH/NdotH) × NdotL ...       (scales F90)
        //
        // LUT coordinates:
        //   x = NdotV (view angle)
        //   y = roughness (perceptual)
        //
        // The final specular color is: F0 × dfg.x + F90 × dfg.y
        // Since F90 ≈ 1.0 for most materials, this simplifies to:
        //   specularColor × dfg.x + dfg.y
        // ---------------------------------------------------------------------
        vec2 dfg = texture(s_dfg_lut, vec2(NdotV, roughness)).rg;
        vec3 specularDFG = specularColor * dfg.x + vec3(dfg.y);

        // ---------------------------------------------------------------------
        // MULTISCATTER ENERGY COMPENSATION
        // ---------------------------------------------------------------------
        // The single-scattering BRDF loses energy at high roughness because it
        // doesn't account for light bouncing multiple times between microfacets.
        // This causes rough metals to appear too dark.
        //
        // Kulla-Conty energy compensation formula:
        //   energyCompensation = 1 + F0 × (1/dfg.y - 1)
        //
        // Where dfg.y represents the directional albedo (r in the equations).
        //
        // IMPROVED CLAMPING
        // Instead of clamping dfg.y to 0.1, we use a smoother approach that
        // prevents the abrupt cutoff while still avoiding divide-by-zero:
        //   energyBias = dfg.y + (1 - dfg.y)³
        // This keeps values close to their original when dfg.y is reasonable,
        // but provides a smooth floor when dfg.y approaches zero.
        //
        // Reference: Kulla & Conty, "Revisiting Physically Based Shading at Imageworks"
        // Reference: Filament docs, "Energy loss in specular reflectance" section
        // ---------------------------------------------------------------------
        float oneMinusDfgY = 1.0 - dfg.y;
        float energyBias = dfg.y + oneMinusDfgY * oneMinusDfgY * oneMinusDfgY;
        energyBias = max(energyBias, 0.001);  // Final safety clamp
        vec3 energyCompensation = 1.0 + specularColor * (1.0 / energyBias - 1.0);

        // ---------------------------------------------------------------------
        // FINAL IBL COMPOSITION
        // ---------------------------------------------------------------------
        // Diffuse IBL: diffuseColor × irradiance
        //   Note: The Lambertian 1/π is typically baked into the irradiance map.
        //
        // Specular IBL: DFG_response × radiance × energy_compensation
        //
        // Accumulated separately for proper clear coat attenuation.
        // ---------------------------------------------------------------------
        Lo_indirect_diffuse += diffuseColor * irradiance;
        Lo_indirect_specular += specularDFG * radiance * energyCompensation;
    }
    #endif

    // =========================================================================
    // SECTION 11: EMISSIVE CONTRIBUTION
    // =========================================================================
    //
    // Emissive materials emit light independent of external lighting.
    // Common uses: screens, neon signs, lava, glowing runes, UI elements.
    //
    // IMPORTANT PROPERTIES:
    //   - NOT affected by shadows (light comes from within)
    //   - NOT affected by ambient occlusion
    //   - Additive contribution to final color
    //   - Can exceed [0,1] range (HDR) for bloom effects
    //
    // In a proper HDR pipeline with tone mapping, high emissive values
    // will bloom naturally when the bloom post-process is applied.
    // -------------------------------------------------------------------------
    #ifdef HAS_EMISSIVE_MAP
        // Sample emissive map and apply strength multiplier
        // Note: Emissive maps should be authored in linear space or sRGB-decoded
        Lo_emissive += texture(s_emissive, v_texcoord).rgb * u_emissiveStrength;
    #endif

    // Add base emissive color (can be combined with or used without map)
    Lo_emissive += u_emissive * u_emissiveStrength;

    // =========================================================================
    // SECTION 12: AMBIENT OCCLUSION (with Specular Occlusion)
    // =========================================================================
    //
    // Ambient occlusion simulates the darkening that occurs in crevices,
    // corners, and areas where ambient light has difficulty reaching.
    //
    // AO APPLICATION RULES: 
    //   - INDIRECT lighting: Apply AO (IBL is approximated ambient)
    //   - DIRECT lighting: Do NOT apply AO (direct lights penetrate crevices)
    //   - Emissive: Do NOT apply AO (light emitted from surface)
    //
    // SPECULAR OCCLUSION
    // Standard AO is designed for diffuse, not specular. Specular reflections
    // come from specific directions, so occlusion affects them differently.
    // We use Lagarde's specular occlusion approximation which considers:
    //   - At grazing angles, specular sees more hemisphere (less occlusion)
    //   - At normal incidence, specular is more focused (more AO effect)
    //
    // The mix() allows partial AO application via u_occlusionStrength,
    // useful for artistic control or when AO maps are too aggressive.
    // -------------------------------------------------------------------------
    float diffuseAO = 1.0;
    float specularAO = 1.0;

    #ifdef HAS_OCCLUSION_MAP
        // Sample AO from red channel (typical ORM texture layout)
        float ao = texture(s_occlusion, v_texcoord).r;

        // Diffuse AO: standard ambient occlusion
        diffuseAO = mix(1.0, ao, u_occlusionStrength);

        // Specular AO: Use Lagarde's method
        // This provides view-dependent specular occlusion that's physically more correct
        float specularAO_raw = compute_specular_occlusion(NdotV, ao, roughness);
        specularAO = mix(1.0, specularAO_raw, u_occlusionStrength);
    #endif

    // Apply AO to indirect lighting (diffuse and specular separately)
    Lo_indirect_diffuse *= diffuseAO;
    Lo_indirect_specular *= specularAO;

    // =========================================================================
    // SECTION 13: CLEAR COAT LAYER (with IBL)
    // =========================================================================
    //
    // Clear coat models a thin, transparent dielectric layer over the base
    // material. This is essential for materials like:
    //   - Automotive paint (base metallic + clear coat)
    //   - Lacquered wood
    //   - Varnished surfaces
    //   - Coated plastics
    //
    // PHYSICAL MODEL:
    // The clear coat is modeled as a second specular lobe that sits on top
    // of the base material. Light interacts with both layers:
    //
    //   1. Some light reflects off the clear coat (specular highlight)
    //   2. Remaining light penetrates and interacts with base material
    //   3. Base material's response is attenuated by clear coat absorption
    //
    // CLEAR COAT BRDF (simplified Cook-Torrance):
    //   f_cc = D_GGX × V_Kelemen × F_Schlick
    //
    // Simplifications:
    //   - Fixed F0 = 0.04 (polyurethane IOR ≈ 1.5)
    //   - Kelemen visibility (cheaper than Smith-GGX)
    //   - Always isotropic
    //   - Uses base layer normal (no separate clear coat normal map)
    //
    // ENERGY CONSERVATION
    // The base layer is attenuated differently for diffuse and specular:
    //   - Diffuse: attenuated by (1 - Fc) once (light enters through clear coat)
    //   - Specular: attenuated by (1 - Fc)² (light enters AND exits through clear coat)
    //
    // Reference: Filament docs, "Clear coat model" section
    // -------------------------------------------------------------------------
    vec3 Lo_clearCoat_direct = vec3(0.0);    // Accumulated clear coat from direct lights
    vec3 Lo_clearCoat_indirect = vec3(0.0);  // Accumulated clear coat from IBL
    float clearCoatAttenuationDiffuse = 1.0;  // (1 - Fc) for diffuse
    float clearCoatAttenuationSpecular = 1.0; // (1 - Fc)² for specular

    if (u_clearCoat > 0.0)
    {
        // ---------------------------------------------------------------------
        // CLEAR COAT ROUGHNESS SETUP
        // ---------------------------------------------------------------------
        // Same clamping as base roughness (0.089 minimum for numerical stability).
        // Clear coat is typically very smooth (0.0-0.2 in practice).
        // ---------------------------------------------------------------------
        float ccRoughness = clamp(u_clearCoatRoughness, 0.089, 1.0);
        float ccAlpha = ccRoughness * ccRoughness;  // Disney roughness remapping

        // ---------------------------------------------------------------------
        // CLEAR COAT F0 (FRESNEL AT NORMAL INCIDENCE)
        // ---------------------------------------------------------------------
        // The clear coat is assumed to be polyurethane (common varnish material).
        // IOR of polyurethane ≈ 1.5
        //
        // F0 = ((n - 1) / (n + 1))² = ((1.5 - 1) / (1.5 + 1))² = 0.04
        //
        // This 4% reflectance matches typical dielectric materials.
        // ---------------------------------------------------------------------
        float ccF0 = 0.04;

        // =====================================================================
        // DIRECTIONAL LIGHT CLEAR COAT EVALUATION
        // =====================================================================
        if (sunlightActive > 0)
        {
            vec3 L = normalize(u_directionalLight.direction);
            vec3 H = normalize(L + V);
            float NdotL = clamp(dot(N, L), 0.001, 1.0);
            float NdotH = clamp(dot(N, H), 0.0, 1.0);
            float LdotH = clamp(dot(L, H), 0.0, 1.0);
            float VdotH = clamp(dot(V, H), 0.0, 1.0);

            // Clear coat BRDF terms:
            // D: GGX normal distribution (use half-precision safe version)
            float ccD = D_GGX(NdotH, ccAlpha, N, H);

            // V: Kelemen visibility (cheaper than Smith-GGX, acceptable for smooth clear coat)
            float ccV = visibility_kelemen(LdotH);

            // F: Schlick Fresnel with fixed F0 = 0.04
            float ccF = ccF0 + (1.0 - ccF0) * pow(1.0 - VdotH, 5.0);

            // Combine: BRDF × light × NdotL × clearCoatStrength
            Lo_clearCoat_direct += u_directionalLight.color * u_directionalLight.amount * NdotL * ccD * ccV * ccF * u_clearCoat;
        }

        // =====================================================================
        // POINT LIGHT CLEAR COAT EVALUATION
        // =====================================================================
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

            // Clear coat BRDF terms (use half-precision safe version)
            float ccD = D_GGX(NdotH, ccAlpha, N, H);
            float ccV = visibility_kelemen(LdotH);
            float ccF = ccF0 + (1.0 - ccF0) * pow(1.0 - VdotH, 5.0);

            Lo_clearCoat_direct += u_pointLights[i].color * attenuation * NdotL * ccD * ccV * ccF * u_clearCoat;
        }

        // =====================================================================
        // CLEAR COAT IBL
        // =====================================================================
        // The clear coat layer should also reflect the environment. We sample
        // the pre-filtered radiance at the clear coat's roughness level and
        // apply the clear coat Fresnel.
        //
        // Since we can't integrate over the hemisphere, we use the reflection
        // vector as the dominant direction (same approximation as base IBL).
        // ---------------------------------------------------------------------
        #ifdef USE_IMAGE_BASED_LIGHTING
        {
            const int NUM_MIP_LEVELS = 6;

            // Clear coat reflection at its own roughness
            float ccMipLevel = ccRoughness * float(NUM_MIP_LEVELS - 1);
            vec3 R = reflect(-V, N);
            vec3 ccCubemapLookup = fix_cube_lookup(R, 512, ccMipLevel);
            vec3 ccRadiance = sRGBToLinear(textureLod(sc_radiance, ccCubemapLookup, ccMipLevel).rgb, DEFAULT_GAMMA) * u_ambientStrength;

            // Clear coat DFG for IBL (use same LUT with clear coat roughness)
            vec2 ccDfg = texture(s_dfg_lut, vec2(NdotV, ccRoughness)).rg;

            // Clear coat is always dielectric with F0 = 0.04
            float ccSpecularDFG = ccF0 * ccDfg.x + ccDfg.y;

            // Apply clear coat strength
            Lo_clearCoat_indirect += ccRadiance * ccSpecularDFG * u_clearCoat;
        }
        #endif

        // ---------------------------------------------------------------------
        // BASE LAYER ATTENUATION 
        // ---------------------------------------------------------------------
        // Energy conservation: Light reflected by clear coat isn't available
        // for the base layer.
        //
        //   - Diffuse: attenuated by (1 - Fc) - light passes through once
        //   - Specular: attenuated by (1 - Fc)² - light passes through twice
        //
        // This uses NdotV (not VdotH) because we're computing the overall
        // energy absorbed by the clear coat layer from the viewing direction.
        // ---------------------------------------------------------------------
        float ccFresnel = ccF0 + (1.0 - ccF0) * pow(1.0 - NdotV, 5.0);
        float Fc = ccFresnel * u_clearCoat;

        clearCoatAttenuationDiffuse = 1.0 - Fc;
        clearCoatAttenuationSpecular = (1.0 - Fc) * (1.0 - Fc);  // = (1 - Fc)²
    }

    // =========================================================================
    // SECTION 14: FINAL COLOR COMPOSITION 
    // =========================================================================
    //
    // Combine all lighting contributions with proper modulation:
    //
    // FORMULA (with proper clear coat energy conservation):
    //   direct = (diffuse × ccAttnDiffuse + specular × ccAttnSpecular + clearCoat) × shadow
    //   indirect = diffuse × ccAttnDiffuse + specular × ccAttnSpecular + clearCoat_ibl
    //   final = direct + indirect + emissive
    //
    // BREAKDOWN:
    //   1. Direct diffuse lighting
    //      - Attenuated by (1 - Fc) once (light enters through clear coat)
    //      - Attenuated by shadows
    //
    //   2. Direct specular lighting
    //      - Attenuated by (1 - Fc)² (light enters AND exits through clear coat)
    //      - Attenuated by shadows
    //
    //   3. Direct clear coat specular
    //      - Attenuated by shadows
    //      - NOT attenuated by clear coat (it IS the clear coat)
    //
    //   4. Indirect diffuse (IBL)
    //      - Attenuated by (1 - Fc) once
    //      - NOT attenuated by shadows
    //      - Already attenuated by diffuse AO
    //
    //   5. Indirect specular (IBL)
    //      - Attenuated by (1 - Fc)²
    //      - NOT attenuated by shadows
    //      - Already attenuated by specular AO
    //
    //   6. Indirect clear coat (IBL)
    //      - NOT attenuated by anything except its own Fresnel
    //
    //   7. Emissive
    //      - NOT attenuated by anything (self-illumination)
    //
    // ENERGY FLOW:
    //   Incoming Light
    //        ↓
    //   Clear Coat (reflects some, transmits rest)
    //        ↓ × (1 - Fc) for diffuse, × (1 - Fc)² for specular
    //   Base Layer (diffuse + specular response)
    //        ↓
    //   + Clear Coat Reflection
    //        ↓
    //   Final Output
    //
    // -------------------------------------------------------------------------

    // Direct lighting with proper clear coat attenuation
    vec3 Lo_direct = Lo_direct_diffuse * clearCoatAttenuationDiffuse
                   + Lo_direct_specular * clearCoatAttenuationSpecular
                   + Lo_clearCoat_direct;

    // Indirect lighting with proper clear coat attenuation
    vec3 Lo_indirect = Lo_indirect_diffuse * clearCoatAttenuationDiffuse
                     + Lo_indirect_specular * clearCoatAttenuationSpecular
                     + Lo_clearCoat_indirect;

    // Final composition
    vec3 finalColor = Lo_direct * shadowVisibility
                    + Lo_indirect
                    + Lo_emissive;

    // =========================================================================
    // SECTION 15: OUTPUT
    // =========================================================================
    //
    // OUTPUT FORMAT: Linear RGB + Alpha
    //
    // The output is in linear color space. A subsequent tone mapping pass
    // should convert to display color space (typically sRGB with gamma curve).
    //
    // Alpha channel: Material opacity for transparency blending.
    // For proper transparency, ensure blend mode is enabled in the renderer.
    //
    // @todo: For transparent materials, consider:
    //   - Premultiplied alpha: color.rgb *= alpha, blend(1, 1-srcAlpha)
    //   - Specular should NOT be multiplied by alpha (remains visible on glass)
    // -------------------------------------------------------------------------
    f_color = vec4(finalColor, u_opacity);
}