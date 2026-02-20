// pt_trace_comp.glsl - Concatenated AFTER pt_common.glsl (no #version here)

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba32f, binding = 1) uniform image2D u_accumulation;

uniform int u_num_prims;
uniform int u_frame_index;
uniform int u_max_bounces;
uniform int u_samples_per_frame;
uniform float u_environment_intensity;
uniform int u_use_environment_map;
uniform sampler1D u_environment_map;
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
    float t_hit;
    int prim_id;
};

float primitive_signed_distance(vec2 p, gpu_sdf_primitive prim)
{
    vec2 local_p = rotate_2d(p - prim.position, -prim.rotation);
    return eval_primitive(local_p, prim);
}

bool intersect_circle_exact(vec2 origin, vec2 dir, gpu_sdf_primitive prim, float min_t, out float t_hit)
{
    vec2 ro = rotate_2d(origin - prim.position, -prim.rotation);
    vec2 rd = rotate_2d(dir, -prim.rotation);
    float r = prim.params.x;

    float b = dot(ro, rd);
    float c = dot(ro, ro) - r * r;
    float h = b * b - c;
    if (h < 0.0) return false;

    float s = sqrt(h);
    float t0 = -b - s;
    float t1 = -b + s;
    float best = 1e30;

    if (t0 >= min_t) best = t0;
    if (t1 >= min_t && t1 < best) best = t1;
    if (best >= 1e30 || best > MAX_MARCH_DIST) return false;

    t_hit = best;
    return true;
}

bool intersect_box_exact(vec2 origin, vec2 dir, gpu_sdf_primitive prim, float min_t, out float t_hit)
{
    vec2 ro = rotate_2d(origin - prim.position, -prim.rotation);
    vec2 rd = rotate_2d(dir, -prim.rotation);

    vec2 h = prim.params.xy;
    float r = clamp(prim.params.z, 0.0, min(h.x, h.y));
    float hx = max(h.x - r, 0.0);
    float hy = max(h.y - r, 0.0);

    float best = 1e30;
    float eps = 1e-7;

    // Side segments
    if (abs(rd.x) > eps)
    {
        float txp = (h.x - ro.x) / rd.x;
        if (txp >= min_t)
        {
            float y = ro.y + txp * rd.y;
            if (abs(y) <= hy + EPSILON_HIT) best = min(best, txp);
        }

        float txn = (-h.x - ro.x) / rd.x;
        if (txn >= min_t)
        {
            float y = ro.y + txn * rd.y;
            if (abs(y) <= hy + EPSILON_HIT) best = min(best, txn);
        }
    }

    if (abs(rd.y) > eps)
    {
        float typ = (h.y - ro.y) / rd.y;
        if (typ >= min_t)
        {
            float x = ro.x + typ * rd.x;
            if (abs(x) <= hx + EPSILON_HIT) best = min(best, typ);
        }

        float tyn = (-h.y - ro.y) / rd.y;
        if (tyn >= min_t)
        {
            float x = ro.x + tyn * rd.x;
            if (abs(x) <= hx + EPSILON_HIT) best = min(best, tyn);
        }
    }

    // Corner arcs
    if (r > 0.0)
    {
        for (int sx_i = 0; sx_i < 2; ++sx_i)
        {
            for (int sy_i = 0; sy_i < 2; ++sy_i)
            {
                float sx = (sx_i == 0) ? -1.0 : 1.0;
                float sy = (sy_i == 0) ? -1.0 : 1.0;
                vec2 c = vec2(sx * hx, sy * hy);

                vec2 oc = ro - c;
                float b = dot(oc, rd);
                float cterm = dot(oc, oc) - r * r;
                float disc = b * b - cterm;
                if (disc < 0.0) continue;

                float s = sqrt(disc);
                float t0 = -b - s;
                float t1 = -b + s;

                if (t0 >= min_t)
                {
                    vec2 p = ro + rd * t0;
                    bool in_quadrant = (sx * (p.x - c.x) >= -EPSILON_HIT) &&
                                       (sy * (p.y - c.y) >= -EPSILON_HIT);
                    if (in_quadrant) best = min(best, t0);
                }
                if (t1 >= min_t)
                {
                    vec2 p = ro + rd * t1;
                    bool in_quadrant = (sx * (p.x - c.x) >= -EPSILON_HIT) &&
                                       (sy * (p.y - c.y) >= -EPSILON_HIT);
                    if (in_quadrant) best = min(best, t1);
                }
            }
        }
    }

    if (best >= 1e30 || best > MAX_MARCH_DIST) return false;
    t_hit = best;
    return true;
}

float refine_root_bisection(vec2 origin, vec2 dir, gpu_sdf_primitive prim, float a, float b, float fa, float fb)
{
    float lo = a;
    float hi = b;
    float flo = fa;

    for (int i = 0; i < 10; ++i)
    {
        float m = 0.5 * (lo + hi);
        float fm = primitive_signed_distance(origin + dir * m, prim);
        if (abs(fm) < EPSILON_HIT) return m;
        if (flo * fm <= 0.0)
        {
            hi = m;
        }
        else
        {
            lo = m;
            flo = fm;
        }
    }

    return 0.5 * (lo + hi);
}

bool intersect_primitive_marched(vec2 origin, vec2 dir, gpu_sdf_primitive prim, float min_t, out float t_hit)
{
    float t = max(min_t, 0.0);
    float sd = primitive_signed_distance(origin + dir * t, prim);

    for (int step = 0; step < MAX_MARCH_STEPS; ++step)
    {
        float step_len = max(abs(sd), EPSILON_HIT * 0.5);
        float t_next = t + step_len;
        if (t_next > MAX_MARCH_DIST) break;

        float sd_next = primitive_signed_distance(origin + dir * t_next, prim);

        if (abs(sd_next) < EPSILON_HIT)
        {
            t_hit = t_next;
            return true;
        }

        if (sd * sd_next < 0.0)
        {
            t_hit = refine_root_bisection(origin, dir, prim, t, t_next, sd, sd_next);
            return true;
        }

        t = t_next;
        sd = sd_next;
    }

    return false;
}

bool intersect_primitive(vec2 origin, vec2 dir, gpu_sdf_primitive prim, float min_t, out float t_hit)
{
    if (prim.prim_type == PRIM_CIRCLE) return intersect_circle_exact(origin, dir, prim, min_t, t_hit);
    if (prim.prim_type == PRIM_BOX) return intersect_box_exact(origin, dir, prim, min_t, t_hit);
    return intersect_primitive_marched(origin, dir, prim, min_t, t_hit);
}

bool find_nearest_intersection(vec2 origin, vec2 dir, int num_prims, float min_t, float max_t, out float out_t, out int out_id)
{
    float best_t = 1e30;
    int best_id = -1;

    for (int i = 0; i < num_prims; ++i)
    {
        float t_i;
        if (intersect_primitive(origin, dir, primitives[i], min_t, t_i))
        {
            if (t_i <= max_t && t_i < best_t)
            {
                best_t = t_i;
                best_id = i;
            }
        }
    }

    out_t = best_t;
    out_id = best_id;
    return best_id >= 0;
}

march_result ray_march(vec2 origin, vec2 dir, int num_prims)
{
    march_result result;
    result.hit = false;
    result.pos = origin;
    result.t_hit = 0.0;
    result.prim_id = -1;

    float min_t = EPSILON_HIT * 2.0;
    float best_t;
    int best_id;
    bool has_hit = find_nearest_intersection(origin, dir, num_prims, min_t, MAX_MARCH_DIST, best_t, best_id);

    if (has_hit)
    {
        result.hit = true;
        result.prim_id = best_id;
        result.t_hit = best_t;
        result.pos = origin + dir * best_t;
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
    if (prim.prim_type == PRIM_BOX)
    {
        float r = prim.params.z;
        return 4.0 * (prim.params.x + prim.params.y) + (TWO_PI - 8.0) * r;
    }
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
        float r = ep.params.z;

        float edge_x = 2.0 * (h.x - r);
        float edge_y = 2.0 * (h.y - r);
        float arc = HALF_PI * r;
        float perim = 2.0 * (edge_x + edge_y) + 4.0 * arc;
        float t = u * perim;

        // Walk: bottom edge, BR arc, right edge, TR arc, top edge, TL arc, left edge, BL arc
        vec2 lp, ln;
        float seg;

        seg = edge_x;
        if (t < seg)
        {
            lp = vec2(t - (h.x - r), -h.y);
            ln = vec2(0.0, -1.0);
        }
        else { t -= seg; seg = arc;
        if (t < seg)
        {
            float a = -HALF_PI + t / r;
            lp = vec2(h.x - r, -h.y + r) + r * vec2(cos(a), sin(a));
            ln = vec2(cos(a), sin(a));
        }
        else { t -= seg; seg = edge_y;
        if (t < seg)
        {
            lp = vec2(h.x, -h.y + r + t);
            ln = vec2(1.0, 0.0);
        }
        else { t -= seg; seg = arc;
        if (t < seg)
        {
            float a = t / r;
            lp = vec2(h.x - r, h.y - r) + r * vec2(cos(a), sin(a));
            ln = vec2(cos(a), sin(a));
        }
        else { t -= seg; seg = edge_x;
        if (t < seg)
        {
            lp = vec2(h.x - r - t, h.y);
            ln = vec2(0.0, 1.0);
        }
        else { t -= seg; seg = arc;
        if (t < seg)
        {
            float a = HALF_PI + t / r;
            lp = vec2(-h.x + r, h.y - r) + r * vec2(cos(a), sin(a));
            ln = vec2(cos(a), sin(a));
        }
        else { t -= seg; seg = edge_y;
        if (t < seg)
        {
            lp = vec2(-h.x, h.y - r - t);
            ln = vec2(-1.0, 0.0);
        }
        else { t -= seg;
            float a = PI + t / max(r, 1e-6);
            lp = vec2(-h.x + r, -h.y + r) + r * vec2(cos(a), sin(a));
            ln = vec2(cos(a), sin(a));
        }}}}}}}

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

    float min_t = EPSILON_HIT * 2.0;
    float max_t = target_dist + EPSILON_SPAWN * 4.0;
    float hit_t;
    int hit_id;

    if (!find_nearest_intersection(from, dir, num_prims, min_t, max_t, hit_t, hit_id)) return true;
    return hit_id == target_prim_id && hit_t >= target_dist - EPSILON_SPAWN * 4.0;
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

    // Initialize medium stack from the ray origin so Beer absorption and IOR
    // are correct when the camera/sample starts inside refractive geometry.
    int inside_ids[MAX_MEDIUM_STACK];
    float inside_sd[MAX_MEDIUM_STACK];
    int inside_count = 0;
    for (int i = 0; i < num_prims; ++i)
    {
        uint mat_i = primitives[i].material_type;
        bool dielectric = (mat_i == MAT_GLASS || mat_i == MAT_WATER || mat_i == MAT_DIAMOND);
        if (!dielectric) continue;

        float sd = primitive_signed_distance(origin, primitives[i]);
        if (sd < 0.0 && inside_count < MAX_MEDIUM_STACK)
        {
            inside_ids[inside_count] = i;
            inside_sd[inside_count] = sd;
            inside_count++;
        }
    }

    // Sort by signed distance ascending (more negative first): outer -> inner.
    for (int a = 0; a < inside_count; ++a)
    {
        for (int b = a + 1; b < inside_count; ++b)
        {
            if (inside_sd[b] < inside_sd[a])
            {
                float tmp_sd = inside_sd[a];
                inside_sd[a] = inside_sd[b];
                inside_sd[b] = tmp_sd;

                int tmp_id = inside_ids[a];
                inside_ids[a] = inside_ids[b];
                inside_ids[b] = tmp_id;
            }
        }
    }

    for (int i = 0; i < inside_count; ++i)
    {
        medium_stack[medium_count] = inside_ids[i];
        medium_count++;
    }

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
        march_result mr = ray_march(origin, dir, num_prims);

        if (!mr.hit)
        {
            vec3 env = vec3(u_environment_intensity);
            if (u_use_environment_map != 0)
            {
                float theta = atan(dir.y, dir.x);
                float u = fract(theta / TWO_PI + 1.0);
                env = texture(u_environment_map, u).rgb * u_environment_intensity;
            }
            radiance += throughput * env;
            break;
        }

        gpu_sdf_primitive prim = primitives[mr.prim_id];
        vec2 outward_normal = calc_primitive_normal(mr.pos, mr.prim_id);
        vec2 normal = outward_normal;
        if (dot(normal, dir) > 0.0) normal = -normal;

        // Beer-Lambert absorption
        if (medium_count > 0 && mr.t_hit > 0.0)
        {
            int current_medium_id = medium_stack[medium_count - 1];
            throughput *= exp(-primitives[current_medium_id].absorption * mr.t_hit);
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
                        float pdf_light_angular = mr.t_hit / (float(num_emitters) * perim * cos_light);
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

    vec3 frame_radiance = vec3(0.0);
    float frame_count = 0.0;
    float aspect = u_resolution.x / u_resolution.y;

    for (int s = 0; s < u_samples_per_frame; ++s)
    {
        int sample_base = u_frame_index * u_samples_per_frame + s;
        rng_state sample_rng = rng_init(uvec2(pixel), uint(sample_base));

        vec2 jitter = lds_sample(cp_offset, sample_base, u_max_bounces + 2, 0);
        float jx = jitter.x - 0.5;
        float jy = jitter.y - 0.5;

        vec2 uv = (vec2(pixel) + vec2(0.5) + vec2(jx, jy)) / u_resolution;
        vec2 ndc = uv * 2.0 - 1.0;

        vec2 world_pos = vec2(ndc.x * aspect, ndc.y) / u_camera_zoom + u_camera_center;

        vec2 angle_xi = lds_sample(cp_offset, sample_base, u_max_bounces + 2, 1);
        float theta = angle_xi.x * TWO_PI;
        vec2 ray_dir = vec2(cos(theta), sin(theta));

        frame_radiance += trace_path(world_pos, ray_dir, u_num_prims, sample_rng, cp_offset, sample_base);
        frame_count += 1.0;
    }

    vec4 prev = imageLoad(u_accumulation, pixel);
    imageStore(u_accumulation, pixel, vec4(prev.rgb + frame_radiance, prev.a + frame_count));
}
