#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec3 inVertexColor;

uniform mat4 u_mvp;
uniform float u_edge_fade_intensity;
uniform float u_edge_fade_distance;
uniform float u_mesh_depth;

out vec3 v_color;
out vec3 v_position;
out float v_fade;

void main()
{
    v_color = inVertexColor;
    v_position = inPosition;

    // Compute edge fade based on Z position
    // Z is centered at 0, mesh_depth is total depth, so normalize to [0,1]
    float z_norm = (inPosition.z / u_mesh_depth) + 0.5;
    float front_fade = smoothstep(0.0, u_edge_fade_distance, z_norm);
    float back_fade = smoothstep(1.0, 1.0 - u_edge_fade_distance, z_norm);
    v_fade = mix(1.0, front_fade * back_fade, u_edge_fade_intensity);

    gl_Position = u_mvp * vec4(inPosition, 1.0);
}
