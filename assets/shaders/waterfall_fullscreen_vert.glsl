#version 450

out vec2 v_texcoord;

void main()
{
    // Fullscreen triangle technique - no vertex buffer needed
    // Vertices: (-1,-1), (3,-1), (-1,3)
    vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2(3.0, -1.0),
        vec2(-1.0, 3.0)
    );

    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
    v_texcoord = positions[gl_VertexID] * 0.5 + 0.5;
}
