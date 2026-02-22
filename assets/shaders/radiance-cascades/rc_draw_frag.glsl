#version 450

in vec2 v_texcoord;
out vec4 frag_color;

uniform sampler2D u_input_texture;
uniform vec2 u_resolution;
uniform vec2 u_from;
uniform vec2 u_to;
uniform float u_radius_squared;
uniform vec4 u_color;
uniform float u_scale;
uniform float u_dpr;
uniform int u_drawing;

float sdf_line_squared(vec2 p, vec2 from, vec2 to)
{
    vec2 to_start = p - from;
    vec2 line = to - from;
    float line_length_squared = dot(line, line);
    float t = clamp(dot(to_start, line) / line_length_squared, 0.0, 1.0);
    vec2 closest_vector = to_start - line * t;
    return dot(closest_vector, closest_vector);
}

void main()
{
    float coef = u_scale;
    float dprs = u_dpr * u_dpr;
    float rsdp = u_radius_squared / dprs;
    float partial = rsdp * coef * coef;
    float max_radius = 100.0;
    float rs = u_scale > 1.0
        ? partial + max(dprs, min(u_dpr * (max_radius + 1.0), partial * 0.1))
        : rsdp;

    vec2 coord = v_texcoord * u_resolution;

    vec4 current = textureLod(u_input_texture, v_texcoord, 0.0);
    if (u_drawing == 1)
    {
        float dist_squared = sdf_line_squared(coord * coef, u_from * coef, u_to * coef);
        if (dist_squared <= rs)
        {
            if (u_color.a > 0.01)
            {
                current = vec4(u_color.rgb * u_color.a, u_color.a);
            }
        }
    }

    frag_color = current;
}
