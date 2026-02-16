#version 450

layout(std430, binding = 0) buffer StarPositions { vec4 star_positions[]; };
layout(std430, binding = 3) buffer StarDensity { float star_density[]; };

uniform mat4 u_viewproj;
uniform float u_particle_size;
uniform vec2 u_viewport; // (width, height)

out float v_density;

void main()
{
    vec4 world_pos = vec4(star_positions[gl_VertexID].xyz, 1.0);
    vec4 clip_pos = u_viewproj * world_pos;
    gl_Position = clip_pos;

    // World-space sized point sprites
    gl_PointSize = u_particle_size * u_viewport.y / clip_pos.w;
    gl_PointSize = max(gl_PointSize, 1.0);

    v_density = star_density[gl_VertexID];
}
