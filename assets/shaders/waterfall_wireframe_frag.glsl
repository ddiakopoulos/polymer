#version 450

in vec3 g_color;
in vec3 g_position;
in float g_fade;
in vec2 g_grid_uv;

uniform float u_line_width;
uniform float u_glow_intensity;
uniform float u_grid_cols;
uniform float u_grid_rows;
uniform float u_grid_density;

out vec4 f_color;

float quad_edge_factor()
{
    // Scale grid UV to cell coordinates, then apply density multiplier
    vec2 cell_pos = g_grid_uv * vec2(u_grid_cols, u_grid_rows) * u_grid_density;

    // Get quad-local UV via fract (computed after interpolation)
    vec2 quad_uv = fract(cell_pos);

    // Distance to all 4 quad edges
    vec4 edge_dist = vec4(
        quad_uv.x,           // left
        1.0 - quad_uv.x,     // right
        quad_uv.y,           // bottom
        1.0 - quad_uv.y      // top
    );

    // Screen-space anti-aliased line width
    vec4 edge_width = fwidth(edge_dist) * u_line_width;
    vec4 edge_aa = smoothstep(vec4(0.0), edge_width, edge_dist);

    // Minimum distance to any edge (0 at edge, 1 in interior)
    return min(min(edge_aa.x, edge_aa.y), min(edge_aa.z, edge_aa.w));
}

void main()
{
    float edge = 1.0 - quad_edge_factor();

    // Holographic glow effect - color from vertex, intensity from glow parameter
    vec3 line_color = g_color * u_glow_intensity;

    // Height-based intensity boost
    float height_factor = clamp(g_position.y * 0.5 + 0.5, 0.0, 1.0);
    line_color *= (0.8 + 0.4 * height_factor);

    // Alpha combines edge detection, fade, and a base glow
    float alpha = edge * g_fade;

    // Discard fragments with very low alpha for performance
    if (alpha < 0.01) discard;

    f_color = vec4(line_color, alpha);
}
