#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in vec3 v_color[];
in vec3 v_position[];
in float v_fade[];

out vec3 g_color;
out vec3 g_position;
out float g_fade;
out vec3 g_barycentric;

void main()
{
    gl_Position = gl_in[0].gl_Position;
    g_color = v_color[0];
    g_position = v_position[0];
    g_fade = v_fade[0];
    g_barycentric = vec3(1.0, 0.0, 0.0);
    EmitVertex();

    gl_Position = gl_in[1].gl_Position;
    g_color = v_color[1];
    g_position = v_position[1];
    g_fade = v_fade[1];
    g_barycentric = vec3(0.0, 1.0, 0.0);
    EmitVertex();

    gl_Position = gl_in[2].gl_Position;
    g_color = v_color[2];
    g_position = v_position[2];
    g_fade = v_fade[2];
    g_barycentric = vec3(0.0, 0.0, 1.0);
    EmitVertex();

    EndPrimitive();
}
