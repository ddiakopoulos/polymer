// pt_trace_comp.glsl - Concatenated AFTER pt_common.glsl (no #version here)

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba32f, binding = 1) uniform image2D u_accumulation;

uniform int u_num_prims;
uniform int u_frame_index;
uniform int u_max_bounces;
uniform int u_samples_per_frame;
uniform float u_environment_intensity;
uniform float u_firefly_clamp;
uniform float u_camera_zoom;
uniform vec2 u_camera_center;
uniform vec2 u_resolution;

#define MAX_EMITTERS 8
#define MAX_MEDIUM_STACK 8

// ============================================================================
// Ray March
// ============================================================================

struct march_result
{
    bool hit;
    vec2 pos;
    float total_dist;
    int prim_id;
};

march_result ray_march(vec2 origin, vec2 dir, int num_prims, bool inside)
{
    march_result result;
    result.hit = false;
    result.pos = origin;
    result.total_dist = 0.0;
    result.prim_id = -1;

    for (int step = 0; step < MAX_MARCH_STEPS; ++step)
    {
        vec2 p = origin + dir * result.total_dist;
        scene_hit sh = eval_scene(p, num_prims);
        float d = inside ? abs(sh.dist) : sh.dist;

        if (d < EPSILON_HIT)
        {
            result.hit = true;
            result.pos = p;
            result.prim_id = sh.prim_id;
            return result;
        }

        result.total_dist += max(d, EPSILON_HIT * 0.5);
        if (result.total_dist > MAX_MARCH_DIST) break;
    }

    return result;
}

// ============================================================================
// NEE Helpers
// ============================================================================

float primitive_perimeter(gpu_sdf_primitive prim)
{
    // Must match actual sampling distribution in sample_emitter_boundary.
    // Only circle and box have proper boundary sampling; all others fall back
    // to circle approximation, so perimeter must match that fallback.
    if (prim.prim_type == PRIM_CIRCLE) return TWO_PI * prim.params.x;
    if (prim.prim_type == PRIM_BOX) return 4.0 * (prim.params.x + prim.params.y);
    return TWO_PI * prim.params.x;
}

struct emitter_sample
{
    vec2 point;
    vec2 normal;
    float pdf;
};

emitter_sample sample_emitter_boundary(int emitter_id, inout rng_state rng)
{
    emitter_sample es;
    gpu_sdf_primitive ep = primitives[emitter_id];
    float u = rand_float(rng);

    if (ep.prim_type == PRIM_CIRCLE)
    {
        float angle = u * TWO_PI;
        float r = ep.params.x;
        vec2 ln = vec2(cos(angle), sin(angle));
        es.point = ep.position + rotate_2d(ln * r, ep.rotation);
        es.normal = rotate_2d(ln, ep.rotation);
        es.pdf = 1.0 / (TWO_PI * r);
    }
    else if (ep.prim_type == PRIM_BOX)
    {
        vec2 h = ep.params.xy;
        float perim = 4.0 * (h.x + h.y);
        float t = u * perim;

        vec2 lp, ln;
        if (t < 2.0 * h.x)
        {
            lp = vec2(t - h.x, -h.y);
            ln = vec2(0.0, -1.0);
        }
        else if (t < 2.0 * h.x + 2.0 * h.y)
        {
            lp = vec2(h.x, t - 2.0 * h.x - h.y);
            ln = vec2(1.0, 0.0);
        }
        else if (t < 4.0 * h.x + 2.0 * h.y)
        {
            lp = vec2(h.x - (t - 2.0 * h.x - 2.0 * h.y), h.y);
            ln = vec2(0.0, 1.0);
        }
        else
        {
            lp = vec2(-h.x, h.y - (t - 4.0 * h.x - 2.0 * h.y));
            ln = vec2(-1.0, 0.0);
        }

        es.point = ep.position + rotate_2d(lp, ep.rotation);
        es.normal = rotate_2d(ln, ep.rotation);
        es.pdf = 1.0 / perim;
    }
    else
    {
        float angle = u * TWO_PI;
        float r = ep.params.x;
        vec2 ln = vec2(cos(angle), sin(angle));
        es.point = ep.position + rotate_2d(ln * r, ep.rotation);
        es.normal = rotate_2d(ln, ep.rotation);
        es.pdf = 1.0 / (TWO_PI * r);
    }

    return es;
}

bool test_visibility(vec2 from, vec2 to, int target_prim_id, int num_prims)
{
    vec2 diff = to - from;
    float target_dist = length(diff);
    if (target_dist < EPSILON_SPAWN) return false;
    vec2 dir = diff / target_dist;

    float t = 0.0;
    for (int step = 0; step < MAX_MARCH_STEPS; ++step)
    {
        if (t >= target_dist - EPSILON_SPAWN) return true;

        vec2 p = from + dir * t;
        scene_hit sh = eval_scene(p, num_prims);

        if (sh.dist < EPSILON_HIT)
        {
            return sh.prim_id == target_prim_id && t >= target_dist - EPSILON_SPAWN * 4.0;
        }

        t += max(sh.dist, EPSILON_HIT * 0.5);
    }
    return false;
}

// ============================================================================
// Path Trace
// ============================================================================

vec3 trace_path(vec2 origin, vec2 dir, int num_prims, inout rng_state rng, vec2 cp_offset, int sample_base)
{
    vec3 throughput = vec3(1.0);
    vec3 radiance = vec3(0.0);
    int medium_stack[MAX_MEDIUM_STACK];
    int medium_count = 0;
    bool prev_was_diffuse = false;
    float prev_bsdf_pdf = 0.0;

    int wavelength_channel = int(rand_float(rng) * 3.0);
    wavelength_channel = min(wavelength_channel, 2);
    float wavelength_um = (wavelength_channel == 0) ? 0.650 : (wavelength_channel == 1) ? 0.550 : 0.450;
    bool hit_dispersive = false;

    // Find emitters once per path
    int emitter_ids[MAX_EMITTERS];
    int num_emitters = 0;
    for (int i = 0; i < num_prims && num_emitters < MAX_EMITTERS; ++i)
    {
        if (primitives[i].emission > 0.0)
        {
            emitter_ids[num_emitters] = i;
            num_emitters++;
        }
    }

    for (int bounce = 0; bounce < u_max_bounces; ++bounce)
    {
        march_result mr = ray_march(origin, dir, num_prims, medium_count > 0);

        if (!mr.hit)
        {
            radiance += throughput * u_environment_intensity;
            break;
        }

        gpu_sdf_primitive prim = primitives[mr.prim_id];
        vec2 outward_normal = calc_normal(mr.pos, num_prims);
        vec2 normal = outward_normal;
        if (dot(normal, dir) > 0.0) normal = -normal;

        // Beer-Lambert absorption
        if (medium_count > 0 && mr.total_dist > 0.0)
        {
            int current_medium_id = medium_stack[medium_count - 1];
            throughput *= exp(-primitives[current_medium_id].absorption * mr.total_dist);
        }

        // Emission
        if (prim.emission > 0.0)
        {
            if (emission_allowed(outward_normal, prim.rotation, prim.emission_half_angle))
            {
                float w = 1.0;
                if (prev_was_diffuse && num_emitters > 0)
                {
                    float cos_light = max(dot(-dir, outward_normal), 0.0);
                    if (cos_light > 0.0)
                    {
                        float perim = primitive_perimeter(prim);
                        float pdf_light_angular = mr.total_dist / (float(num_emitters) * perim * cos_light);
                        w = prev_bsdf_pdf / (prev_bsdf_pdf + pdf_light_angular);
                    }
                }
                radiance += throughput * prim.emission * prim.albedo * w;
                break;
            }
            // Surface still exists but not emitting in this direction — continue bouncing
        }

        uint mat = prim.material_type;

        if (mat == MAT_DIFFUSE)
        {
            vec2 xi = lds_sample(cp_offset, sample_base, u_max_bounces + 2, bounce + 2);

            // ================================================================
            // NEE: sample an emitter directly
            // ================================================================
            if (num_emitters > 0)
            {
                int eidx = int(xi.y * float(num_emitters));
                eidx = min(eidx, num_emitters - 1);
                int emitter_id = emitter_ids[eidx];

                emitter_sample es = sample_emitter_boundary(emitter_id, rng);
                gpu_sdf_primitive ep = primitives[emitter_id];

                if (emission_allowed(es.normal, ep.rotation, ep.emission_half_angle))
                {
                    vec2 to_light = es.point - mr.pos;
                    float light_dist = length(to_light);

                    if (light_dist > EPSILON_SPAWN * 2.0)
                    {
                        vec2 light_dir = to_light / light_dist;
                        float cos_surface = max(dot(light_dir, normal), 0.0);
                        float cos_light = max(dot(-light_dir, es.normal), 0.0);

                        if (cos_surface > 0.0 && cos_light > 0.0)
                        {
                            vec2 shadow_origin = mr.pos + normal * EPSILON_SPAWN;
                            if (test_visibility(shadow_origin, es.point, emitter_id, num_prims))
                            {
                                vec3 Le = ep.emission * ep.albedo;

                                float pdf_area = 1.0 / (float(num_emitters) * primitive_perimeter(ep));
                                float pdf_light_angular = pdf_area * light_dist / cos_light;
                                float pdf_bsdf = cos_surface / 2.0;

                                float w_light = pdf_light_angular / (pdf_light_angular + pdf_bsdf);

                                radiance += throughput * (prim.albedo / 2.0) * cos_surface * Le / pdf_light_angular * w_light;
                            }
                        }
                    }
                }
            }

            // ================================================================
            // BSDF sample
            // ================================================================
            throughput *= prim.albedo;
            dir = sample_cosine_half_circle_u(xi.x, normal);
            origin = mr.pos + normal * EPSILON_SPAWN;
            prev_was_diffuse = true;
            prev_bsdf_pdf = max(dot(dir, normal), 0.0) / 2.0;
        }
        else if (mat == MAT_MIRROR)
        {
            throughput *= prim.albedo;
            dir = reflect_2d(dir, normal);
            origin = mr.pos + normal * EPSILON_SPAWN;
            prev_was_diffuse = false;
        }
        else // Dielectric
        {
            hit_dispersive = true;

            float ior_hit = cauchy_ior(prim.ior_base, prim.cauchy_b, prim.cauchy_c, wavelength_um);
            bool front_face = dot(dir, outward_normal) < 0.0;

            float n1 = 1.0;
            if (medium_count > 0)
            {
                int current_medium_id = medium_stack[medium_count - 1];
                gpu_sdf_primitive current_medium = primitives[current_medium_id];
                n1 = cauchy_ior(current_medium.ior_base, current_medium.cauchy_b, current_medium.cauchy_c, wavelength_um);
            }

            float n2 = n1;
            if (front_face)
            {
                n2 = ior_hit;
            }
            else
            {
                int exit_index = -1;
                for (int i = medium_count - 1; i >= 0; --i)
                {
                    if (medium_stack[i] == mr.prim_id)
                    {
                        exit_index = i;
                        break;
                    }
                }

                if (exit_index > 0)
                {
                    int outer_id = medium_stack[exit_index - 1];
                    gpu_sdf_primitive outer_medium = primitives[outer_id];
                    n2 = cauchy_ior(outer_medium.ior_base, outer_medium.cauchy_b, outer_medium.cauchy_c, wavelength_um);
                }
                else
                {
                    n2 = 1.0;
                }
            }

            float cos_theta = abs(dot(dir, normal));
            float R = fresnel_schlick(cos_theta, n1, n2);

            vec2 refracted_dir;
            bool can_refract = refract_2d(dir, normal, n1 / n2, refracted_dir);

            if (!can_refract || rand_float(rng) < R)
            {
                dir = reflect_2d(dir, normal);
                origin = mr.pos + normal * EPSILON_SPAWN;
            }
            else
            {
                dir = normalize(refracted_dir);
                origin = mr.pos - normal * EPSILON_SPAWN;

                if (front_face)
                {
                    if (medium_count < MAX_MEDIUM_STACK)
                    {
                        medium_stack[medium_count] = mr.prim_id;
                        medium_count++;
                    }
                }
                else
                {
                    int exit_index = -1;
                    for (int i = medium_count - 1; i >= 0; --i)
                    {
                        if (medium_stack[i] == mr.prim_id)
                        {
                            exit_index = i;
                            break;
                        }
                    }

                    if (exit_index >= 0)
                    {
                        for (int i = exit_index; i < medium_count - 1; ++i)
                        {
                            medium_stack[i] = medium_stack[i + 1];
                        }
                        medium_count--;
                    }
                }
            }
            prev_was_diffuse = false;
        }

        // Russian Roulette (starting at bounce 3)
        if (bounce > 2)
        {
            float p_survive = min(max(throughput.x, max(throughput.y, throughput.z)), 0.95);
            if (p_survive <= 1e-4) break;
            if (rand_float(rng) > p_survive) break;
            throughput /= p_survive;
        }
    }

    // Firefly clamping
    float lum = dot(radiance, vec3(0.2126, 0.7152, 0.0722));
    if (lum > u_firefly_clamp) radiance *= u_firefly_clamp / lum;

    // Dispersion: write only the selected channel, weighted by 3
    if (hit_dispersive)
    {
        vec3 dispersed = vec3(0.0);
        dispersed[wavelength_channel] = radiance[wavelength_channel] * 3.0;
        return dispersed;
    }

    return radiance;
}

// ============================================================================
// Main
// ============================================================================

void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = ivec2(u_resolution);
    if (pixel.x >= size.x || pixel.y >= size.y) return;

    vec2 cp_offset = cranley_patterson_offset(uvec2(pixel));
    rng_state rng = rng_init(uvec2(pixel), uint(u_frame_index));

    vec3 frame_radiance = vec3(0.0);
    float frame_count = 0.0;
    float aspect = u_resolution.x / u_resolution.y;

    for (int s = 0; s < u_samples_per_frame; ++s)
    {
        int sample_base = u_frame_index * u_samples_per_frame + s;

        vec2 jitter = lds_sample(cp_offset, sample_base, u_max_bounces + 2, 0);
        float jx = jitter.x - 0.5;
        float jy = jitter.y - 0.5;

        vec2 uv = (vec2(pixel) + vec2(0.5) + vec2(jx, jy)) / u_resolution;
        vec2 ndc = uv * 2.0 - 1.0;

        vec2 world_pos = vec2(ndc.x * aspect, ndc.y) / u_camera_zoom + u_camera_center;

        vec2 angle_xi = lds_sample(cp_offset, sample_base, u_max_bounces + 2, 1);
        float theta = angle_xi.x * TWO_PI;
        vec2 ray_dir = vec2(cos(theta), sin(theta));

        frame_radiance += trace_path(world_pos, ray_dir, u_num_prims, rng, cp_offset, sample_base);
        frame_count += 1.0;
    }

    vec4 prev = imageLoad(u_accumulation, pixel);
    imageStore(u_accumulation, pixel, vec4(prev.rgb + frame_radiance, prev.a + frame_count));
}
