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
out vec3 g_edge_mask;  // 1.0 = draw edge, 0.0 = hide edge (diagonal)

uniform float u_grid_cols;
uniform float u_grid_rows;

// Check if edge between two vertices is a diagonal (differs in both U and V)
bool is_diagonal_edge(vec2 uv0, vec2 uv1)
{
    vec2 cell0 = uv0 * vec2(u_grid_cols, u_grid_rows);
    vec2 cell1 = uv1 * vec2(u_grid_cols, u_grid_rows);
    vec2 diff = abs(cell0 - cell1);
    // Diagonal if both components differ (not axis-aligned)
    return (diff.x > 0.5 && diff.y > 0.5);
}

void main()
{
    // Determine which edges are diagonals
    // Edge opposite to vertex i connects vertices j and k
    // Vertex 0: opposite edge is 1-2
    // Vertex 1: opposite edge is 0-2
    // Vertex 2: opposite edge is 0-1
    vec3 edge_mask = vec3(
        is_diagonal_edge(v_grid_uv[1], v_grid_uv[2]) ? 0.0 : 1.0,  // edge 1-2
        is_diagonal_edge(v_grid_uv[0], v_grid_uv[2]) ? 0.0 : 1.0,  // edge 0-2
        is_diagonal_edge(v_grid_uv[0], v_grid_uv[1]) ? 0.0 : 1.0   // edge 0-1
    );

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
