#version 450

// pt_common.glsl - Prepended to compute shader via string concatenation
// Contains: constants, structs, SSBO, RNG, SDF primitives, scene eval, normals, sampling

// ============================================================================
// Constants
// ============================================================================

#define PI 3.14159265358979323846
#define TWO_PI 6.28318530717958647692
#define HALF_PI 1.57079632679489661923
#define INV_PI 0.31830988618379067154

#define EPSILON_HIT 1e-4
#define EPSILON_GRAD 5e-4
#define EPSILON_SPAWN 2e-3
#define MAX_MARCH_STEPS 256
#define MAX_MARCH_DIST 100.0

#define PRIM_CIRCLE  0u
#define PRIM_BOX     1u
#define PRIM_CAPSULE 2u
#define PRIM_SEGMENT 3u
#define PRIM_LENS    4u
#define PRIM_NGON    5u

#define MAT_DIFFUSE  0u
#define MAT_MIRROR   1u
#define MAT_GLASS    2u
#define MAT_WATER    3u
#define MAT_DIAMOND  4u

// ============================================================================
// GPU SDF Primitive (80 bytes, std430 compatible)
// ============================================================================

struct gpu_sdf_primitive
{
    vec2 position;       // offset 0
    float rotation;      // offset 8
    uint prim_type;      // offset 12
    vec4 params;         // offset 16 (16-aligned)
    uint material_type;  // offset 32
    float ior_base;      // offset 36
    float cauchy_b;      // offset 40
    float cauchy_c;      // offset 44
    vec3 albedo;         // offset 48 (16-aligned)
    float emission;      // offset 60
    vec3 absorption;           // offset 64 (16-aligned)
    float emission_half_angle; // offset 76 (default PI = omnidirectional)
};

// SSBO - must precede eval_scene which references primitives[]
layout(std430, binding = 0) readonly buffer PrimitiveBuffer
{
    gpu_sdf_primitive primitives[];
};

// ============================================================================
// RNG - PCG Hash
// ============================================================================

uint pcg_hash(uint state)
{
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

struct rng_state
{
    uint s;
};

float rand_float(inout rng_state rng)
{
    rng.s = pcg_hash(rng.s);
    return float(rng.s) / 4294967295.0;
}

rng_state rng_init(uvec2 pixel, uint frame)
{
    rng_state rng;
    rng.s = pcg_hash(pixel.x * 1973u + pixel.y * 9277u + frame * 26699u);
    return rng;
}

// ============================================================================
// R2 Low-Discrepancy Sequence + Cranley-Patterson Rotation
// ============================================================================

#define R2_ALPHA1 0.7548776662466927
#define R2_ALPHA2 0.5698402909980532

vec2 r2_sequence(uint n)
{
    return fract(vec2(0.5 + R2_ALPHA1 * float(n),
                      0.5 + R2_ALPHA2 * float(n)));
}

vec2 cranley_patterson_offset(uvec2 pixel)
{
    uint h1 = pcg_hash(pixel.x * 1973u + pixel.y * 9277u + 1u);
    uint h2 = pcg_hash(pixel.x * 2969u + pixel.y * 7541u + 2u);
    return vec2(float(h1) / 4294967295.0, float(h2) / 4294967295.0);
}

vec2 lds_sample(vec2 cp_offset, int sample_base, int stride, int slot)
{
    uint n = uint(sample_base) * uint(stride) + uint(slot);
    return fract(r2_sequence(n) + cp_offset);
}

// ============================================================================
// 2D SDF Primitives
// ============================================================================

float sdf_circle(vec2 p, float r)
{
    return length(p) - r;
}

float sdf_box(vec2 p, vec2 half_size)
{
    vec2 d = abs(p) - half_size;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float sdf_capsule(vec2 p, float r, float half_len)
{
    p.x -= clamp(p.x, -half_len, half_len);
    return length(p) - r;
}

float sdf_segment(vec2 p, float half_len, float thickness)
{
    p.x -= clamp(p.x, -half_len, half_len);
    return length(p) - thickness;
}

float sdf_lens(vec2 p, float r1, float r2, float d, float aperture_half_height)
{
    float half_d = d * 0.5;
    float ar1 = max(abs(r1), 1e-4);
    float ar2 = max(abs(r2), 1e-4);

    // Vertex positions are fixed at x = +/- half_d. The sign of r controls
    // curvature direction: r > 0 is convex, r < 0 is concave.
    vec2 c1 = vec2(-half_d + r1, 0.0);
    vec2 c2 = vec2(half_d - r2, 0.0);

    float side1 = length(p - c1) - ar1;
    float side2 = length(p - c2) - ar2;

    if (r1 < 0.0) side1 = -side1;
    if (r2 < 0.0) side2 = -side2;

    // Finite aperture keeps biconcave/meniscus forms bounded.
    float aperture = (aperture_half_height > 0.0) ? aperture_half_height : (min(ar1, ar2) * 0.98);
    float cap = abs(p.y) - aperture;

    return max(max(side1, side2), cap);
}

float sdf_ngon(vec2 p, float r, float sides)
{
    float n = max(sides, 3.0);
    float an = PI / n;
    float he = r * cos(an);
    float angle = atan(p.y, p.x);
    float sector = mod(angle + an, 2.0 * an) - an;
    vec2 q = length(p) * vec2(cos(sector), abs(sin(sector)));
    return q.x - he;
}

// ============================================================================
// Scene Evaluation
// ============================================================================

struct scene_hit
{
    float dist;
    int prim_id;
};

vec2 rotate_2d(vec2 p, float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return vec2(c * p.x + s * p.y, -s * p.x + c * p.y);
}

float eval_primitive(vec2 local_p, gpu_sdf_primitive prim)
{
    switch (prim.prim_type)
    {
        case PRIM_CIRCLE:  return sdf_circle(local_p, prim.params.x);
        case PRIM_BOX:     return sdf_box(local_p, prim.params.xy);
        case PRIM_CAPSULE: return sdf_capsule(local_p, prim.params.x, prim.params.y);
        case PRIM_SEGMENT: return sdf_segment(local_p, prim.params.x, prim.params.y);
        case PRIM_LENS:    return sdf_lens(local_p, prim.params.x, prim.params.y, prim.params.z, prim.params.w);
        case PRIM_NGON:    return sdf_ngon(local_p, prim.params.x, prim.params.y);
        default:           return 1e10;
    }
}

scene_hit eval_scene(vec2 p, int num_prims)
{
    scene_hit hit;
    hit.dist = 1e10;
    hit.prim_id = -1;

    for (int i = 0; i < num_prims; ++i)
    {
        vec2 local_p = rotate_2d(p - primitives[i].position, -primitives[i].rotation);
        float d = eval_primitive(local_p, primitives[i]);
        if (d < hit.dist)
        {
            hit.dist = d;
            hit.prim_id = i;
        }
    }
    return hit;
}

vec2 calc_normal(vec2 p, int num_prims)
{
    float dx = eval_scene(p + vec2(EPSILON_GRAD, 0.0), num_prims).dist
             - eval_scene(p - vec2(EPSILON_GRAD, 0.0), num_prims).dist;
    float dy = eval_scene(p + vec2(0.0, EPSILON_GRAD), num_prims).dist
             - eval_scene(p - vec2(0.0, EPSILON_GRAD), num_prims).dist;
    return normalize(vec2(dx, dy));
}

// ============================================================================
// Sampling Helpers
// ============================================================================

vec2 sample_cosine_half_circle(inout rng_state rng, vec2 normal)
{
    float u = rand_float(rng);
    // Inverse CDF for p(theta) = 0.5 * cos(theta), theta in [-pi/2, pi/2].
    float theta = asin(2.0 * u - 1.0);
    vec2 local_dir = vec2(cos(theta), sin(theta));
    vec2 tangent = vec2(-normal.y, normal.x);
    return local_dir.x * normal + local_dir.y * tangent;
}

vec2 sample_cosine_half_circle_u(float u, vec2 normal)
{
    float theta = asin(2.0 * u - 1.0);
    vec2 local_dir = vec2(cos(theta), sin(theta));
    vec2 tangent = vec2(-normal.y, normal.x);
    return local_dir.x * normal + local_dir.y * tangent;
}

float cauchy_ior(float ior_base, float cauchy_b, float cauchy_c, float lambda_um)
{
    float l2 = lambda_um * lambda_um;
    return ior_base + cauchy_b / l2 + cauchy_c / (l2 * l2);
}

float fresnel_schlick(float cos_theta, float n1, float n2)
{
    float r0 = (n1 - n2) / (n1 + n2);
    r0 *= r0;
    float x = 1.0 - cos_theta;
    return r0 + (1.0 - r0) * x * x * x * x * x;
}

vec2 reflect_2d(vec2 d, vec2 n)
{
    return d - 2.0 * dot(d, n) * n;
}

bool refract_2d(vec2 d, vec2 n, float eta, out vec2 refracted)
{
    float cos_i = -dot(d, n);
    float sin2_t = eta * eta * (1.0 - cos_i * cos_i);
    if (sin2_t > 1.0) return false;
    float cos_t = sqrt(1.0 - sin2_t);
    refracted = eta * d + (eta * cos_i - cos_t) * n;
    return true;
}

bool emission_allowed(vec2 outward_normal, float rotation, float half_angle)
{
    if (half_angle >= PI) return true;
    vec2 forward = vec2(cos(rotation), sin(rotation));
    return dot(outward_normal, forward) > cos(half_angle);
}
