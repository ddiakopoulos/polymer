#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in vec3 v_color[];
in vec3 v_position[];
in float v_fade[];
in vec2 v_grid_uv[];

out vec3 g_color;
out vec3 g_position;
out float g_fade;
out vec3 g_barycentric;
out vec3 g_edge_mask;  // 1.0 = draw edge, 0.0 = hide edge

uniform float u_grid_cols;
uniform float u_grid_rows;
uniform float u_grid_density;

// Check if edge between two vertices is a diagonal (differs in both U and V)
bool is_diagonal_edge(vec2 uv0, vec2 uv1)
{
    vec2 cell0 = uv0 * vec2(u_grid_cols, u_grid_rows);
    vec2 cell1 = uv1 * vec2(u_grid_cols, u_grid_rows);
    vec2 diff = abs(cell0 - cell1);
    return (diff.x > 0.5 && diff.y > 0.5);
}

// Check if edge should be drawn based on grid density (for sparser grids)
bool should_draw_edge(vec2 uv0, vec2 uv1)
{
    if (u_grid_density >= 1.0) return true;  // Full density, draw all edges

    float step = 1.0 / u_grid_density;  // e.g., density=0.5 -> step=2

    vec2 cell0 = uv0 * vec2(u_grid_cols, u_grid_rows);
    vec2 cell1 = uv1 * vec2(u_grid_cols, u_grid_rows);
    vec2 diff = abs(cell0 - cell1);

    // Vertical edge (same column, different rows)
    if (diff.x < 0.5 && diff.y > 0.5)
    {
        float col = round(cell0.x);
        return mod(col, step) < 0.5;
    }

    // Horizontal edge (same row, different columns)
    if (diff.y < 0.5 && diff.x > 0.5)
    {
        float row = round(cell0.y);
        return mod(row, step) < 0.5;
    }

    return true;
}

void main()
{
    // Determine which edges to draw
    // Edge opposite to vertex i connects vertices j and k
    vec3 edge_mask;

    // Edge 1-2 (opposite vertex 0)
    bool diag_12 = is_diagonal_edge(v_grid_uv[1], v_grid_uv[2]);
    bool draw_12 = should_draw_edge(v_grid_uv[1], v_grid_uv[2]);
    edge_mask.x = (diag_12 || !draw_12) ? 0.0 : 1.0;

    // Edge 0-2 (opposite vertex 1)
    bool diag_02 = is_diagonal_edge(v_grid_uv[0], v_grid_uv[2]);
    bool draw_02 = should_draw_edge(v_grid_uv[0], v_grid_uv[2]);
    edge_mask.y = (diag_02 || !draw_02) ? 0.0 : 1.0;

    // Edge 0-1 (opposite vertex 2)
    bool diag_01 = is_diagonal_edge(v_grid_uv[0], v_grid_uv[1]);
    bool draw_01 = should_draw_edge(v_grid_uv[0], v_grid_uv[1]);
    edge_mask.z = (diag_01 || !draw_01) ? 0.0 : 1.0;

    // Vertex 0
    gl_Position = gl_in[0].gl_Position;
    g_color = v_color[0];
    g_position = v_position[0];
    g_fade = v_fade[0];
    g_barycentric = vec3(1.0, 0.0, 0.0);
    g_edge_mask = edge_mask;
    EmitVertex();

    // Vertex 1
    gl_Position = gl_in[1].gl_Position;
    g_color = v_color[1];
    g_position = v_position[1];
    g_fade = v_fade[1];
    g_barycentric = vec3(0.0, 1.0, 0.0);
    g_edge_mask = edge_mask;
    EmitVertex();

    // Vertex 2
    gl_Position = gl_in[2].gl_Position;
    g_color = v_color[2];
    g_position = v_position[2];
    g_fade = v_fade[2];
    g_barycentric = vec3(0.0, 0.0, 1.0);
    g_edge_mask = edge_mask;
    EmitVertex();

    EndPrimitive();
}
