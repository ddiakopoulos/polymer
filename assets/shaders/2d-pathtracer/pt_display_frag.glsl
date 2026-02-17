// pt_display_frag.glsl
// Prepended with pt_common.glsl (which provides #version 450, SDF functions, SSBO, etc.)

in vec2 v_texcoord;
out vec4 frag_color;

uniform sampler2D u_accumulation_tex;
uniform float u_exposure;

// Debug overlay uniforms
uniform float u_camera_zoom;
uniform vec2 u_camera_center;
uniform vec2 u_resolution;
uniform int u_num_prims;
uniform int u_selected_prim;
uniform int u_debug_overlay;

// ACES filmic tonemapping
vec3 aces_tonemap(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec4 accum = texture(u_accumulation_tex, v_texcoord);

    vec3 color = vec3(0.0);
    if (accum.a >= 1.0) color = accum.rgb / accum.a;

    color *= u_exposure;
    color = aces_tonemap(color);

    // SDF debug wireframe overlay
    if (u_debug_overlay != 0 && u_num_prims > 0)
    {
        float aspect = u_resolution.x / u_resolution.y;
        vec2 ndc = v_texcoord * 2.0 - 1.0;
        vec2 world_pos = vec2(ndc.x * aspect, ndc.y) / u_camera_zoom + u_camera_center;

        float pixel_size = 2.0 / (u_camera_zoom * u_resolution.y);
        float outline_threshold = pixel_size * 2.0;

        for (int i = 0; i < u_num_prims; ++i)
        {
            vec2 local_p = rotate_2d(world_pos - primitives[i].position, -primitives[i].rotation);
            float d = abs(eval_primitive(local_p, primitives[i]));

            if (d < outline_threshold)
            {
                float t = 1.0 - d / outline_threshold;
                t = t * t;

                if (i == u_selected_prim)
                {
                    color = mix(color, vec3(0.0, 0.9, 1.0), t * 0.9);
                }
                else
                {
                    color = mix(color, vec3(0.5, 0.5, 0.5), t * 0.35);
                }
            }
        }
    }

    frag_color = vec4(color, 1.0);
}
