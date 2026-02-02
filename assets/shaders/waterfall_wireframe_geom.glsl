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
out vec2 g_grid_uv;

void main()
{
    for (int i = 0; i < 3; ++i)
    {
        gl_Position = gl_in[i].gl_Position;
        g_color = v_color[i];
        g_position = v_position[i];
        g_fade = v_fade[i];
        g_grid_uv = v_grid_uv[i];  // Pass through raw grid UV for interpolation
        EmitVertex();
    }
    EndPrimitive();
}
