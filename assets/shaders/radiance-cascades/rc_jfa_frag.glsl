#version 450

in vec2 v_texcoord;
out vec2 frag_seed;

uniform sampler2D u_input_texture;
uniform vec2 u_resolution;
uniform vec2 u_one_over_size;
uniform float u_offset;
uniform int u_skip;

void main()
{
    if (u_skip == 1)
    {
        frag_seed = v_texcoord;
        return;
    }

    vec2 nearest_seed = vec2(-1.0);
    float nearest_dist = 999999.9;
    vec2 pre = u_offset * u_one_over_size;

    // Start with center texel
    vec2 sample_value = texelFetch(u_input_texture, ivec2(v_texcoord * u_resolution), 0).xy;
    vec2 sample_seed = sample_value.xy;

    if (sample_seed.x > 0.0 || sample_seed.y > 0.0)
    {
        vec2 diff = sample_seed - v_texcoord;
        float dist = dot(diff, diff);
        if (dist < nearest_dist)
        {
            nearest_dist = dist;
            nearest_seed.xy = sample_value.xy;
        }
    }

    // Check 3x3 neighborhood (skip center)
    for (float y = -1.0; y <= 1.0; y += 1.0)
    {
        for (float x = -1.0; x <= 1.0; x += 1.0)
        {
            if (x == 0.0 && y == 0.0) continue;

            vec2 sample_uv = v_texcoord + vec2(x, y) * pre;

            if (sample_uv.x < 0.0 || sample_uv.x > 1.0 || sample_uv.y < 0.0 || sample_uv.y > 1.0) continue;

            vec2 sv = texelFetch(u_input_texture, ivec2(sample_uv * u_resolution), 0).xy;
            vec2 ss = sv.xy;

            if (ss.x > 0.0 || ss.y > 0.0)
            {
                vec2 diff = ss - v_texcoord;
                float dist = dot(diff, diff);
                if (dist < nearest_dist)
                {
                    nearest_dist = dist;
                    nearest_seed.xy = sv.xy;
                }
            }
        }
    }

    frag_seed = nearest_seed;
}
