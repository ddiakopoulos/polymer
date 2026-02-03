#version 450

uniform sampler2D s_current;
uniform sampler2D s_history;
uniform sampler2D s_velocity;
uniform sampler2D s_depth;

uniform vec4 u_jitter_uv;      // xy=current, zw=previous
uniform vec2 u_texel_size;
uniform float u_feedback_min;
uniform float u_feedback_max;

in vec2 v_texcoord;
out vec4 f_color;

const float FLT_EPS = 0.00000001;

// RGB to YCoCg
vec3 rgb_to_ycocg(vec3 rgb)
{
    return vec3(
         rgb.r * 0.25 + rgb.g * 0.5 + rgb.b * 0.25,
         rgb.r * 0.5  - rgb.b * 0.5,
        -rgb.r * 0.25 + rgb.g * 0.5 - rgb.b * 0.25
    );
}

vec3 ycocg_to_rgb(vec3 ycocg)
{
    return clamp(vec3(
        ycocg.x + ycocg.y - ycocg.z,
        ycocg.x + ycocg.z,
        ycocg.x - ycocg.y - ycocg.z
    ), 0.0, 1.0);
}

vec4 sample_color(sampler2D tex, vec2 uv)
{
    vec4 c = texture(tex, uv);
    return vec4(rgb_to_ycocg(c.rgb), c.a);
}

// AABB clipping (Salvi's variance clipping)
vec4 clip_aabb(vec3 aabb_min, vec3 aabb_max, vec4 avg, vec4 history)
{
    vec3 p_clip = 0.5 * (aabb_max + aabb_min);
    vec3 e_clip = 0.5 * (aabb_max - aabb_min) + FLT_EPS;
    vec4 v_clip = history - vec4(p_clip, avg.w);
    vec3 v_unit = v_clip.xyz / e_clip;
    vec3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));
    if (ma_unit > 1.0)
        return vec4(p_clip, avg.w) + v_clip / ma_unit;
    return history;
}

// Find closest depth in 3x3 for dilation
vec3 find_closest_fragment_3x3(vec2 uv)
{
    vec3 dmin = vec3(0.0, 0.0, texture(s_depth, uv).r);

    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            vec2 offset = vec2(float(x), float(y));
            float d = texture(s_depth, uv + offset * u_texel_size).r;
            if (d < dmin.z)
            {
                dmin = vec3(offset, d);
            }
        }
    }
    return vec3(uv + dmin.xy * u_texel_size, dmin.z);
}

void main()
{
    // Depth-based dilation for velocity
    vec3 closest = find_closest_fragment_3x3(v_texcoord);
    vec2 velocity = texture(s_velocity, closest.xy).rg;

    // Sample current (unjittered)
    vec2 current_uv = v_texcoord - u_jitter_uv.xy;
    vec4 current_color = sample_color(s_current, current_uv);

    // Sample history (reprojected)
    vec2 history_uv = v_texcoord - velocity;

    // Bounds check
    if (any(lessThan(history_uv, vec2(0.0))) || any(greaterThan(history_uv, vec2(1.0))))
    {
        f_color = vec4(ycocg_to_rgb(current_color.rgb), 1.0);
        return;
    }

    vec4 history_color = sample_color(s_history, history_uv);

    // 3x3 neighborhood min/max
    vec2 du = vec2(u_texel_size.x, 0.0);
    vec2 dv = vec2(0.0, u_texel_size.y);
    vec2 uv = current_uv;

    vec4 ctl = sample_color(s_current, uv - dv - du);
    vec4 ctc = sample_color(s_current, uv - dv);
    vec4 ctr = sample_color(s_current, uv - dv + du);
    vec4 cml = sample_color(s_current, uv - du);
    vec4 cmc = current_color;
    vec4 cmr = sample_color(s_current, uv + du);
    vec4 cbl = sample_color(s_current, uv + dv - du);
    vec4 cbc = sample_color(s_current, uv + dv);
    vec4 cbr = sample_color(s_current, uv + dv + du);

    vec4 cmin = min(ctl, min(ctc, min(ctr, min(cml, min(cmc, min(cmr, min(cbl, min(cbc, cbr))))))));
    vec4 cmax = max(ctl, max(ctc, max(ctr, max(cml, max(cmc, max(cmr, max(cbl, max(cbc, cbr))))))));
    vec4 cavg = (ctl + ctc + ctr + cml + cmc + cmr + cbl + cbc + cbr) / 9.0;

    // Rounded 3x3 (tighter bounds)
    vec4 cmin5 = min(ctc, min(cml, min(cmc, min(cmr, cbc))));
    vec4 cmax5 = max(ctc, max(cml, max(cmc, max(cmr, cbc))));
    cmin = 0.5 * (cmin + cmin5);
    cmax = 0.5 * (cmax + cmax5);

    // Shrink chroma bounds
    float chroma_extent = 0.25 * 0.5 * (cmax.r - cmin.r);
    vec2 chroma_center = current_color.gb;
    cmin.gb = chroma_center - chroma_extent;
    cmax.gb = chroma_center + chroma_extent;

    // Clip history to AABB
    vec4 clipped = clip_aabb(cmin.rgb, cmax.rgb, clamp(cavg, cmin, cmax), history_color);

    // Luminance-based feedback
    float lum_curr = current_color.r;
    float lum_hist = clipped.r;
    float diff = abs(lum_curr - lum_hist) / max(lum_curr, max(lum_hist, 0.2));
    float weight = 1.0 - diff;
    float feedback = mix(u_feedback_min, u_feedback_max, weight * weight);

    // Blend
    vec4 result = mix(current_color, clipped, feedback);

    // Dithering
    float noise = fract(sin(dot(v_texcoord, vec2(12.9898, 78.233))) * 43758.5453);

    f_color = vec4(ycocg_to_rgb(result.rgb) + noise / 510.0, 1.0);
}
