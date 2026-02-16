#version 450

layout(std430, binding = 4) buffer CloudPositions { vec4 cloud_positions[]; };
layout(std430, binding = 6) buffer CloudColors { vec4 cloud_colors[]; };
layout(std430, binding = 7) buffer CloudSizes { float cloud_sizes[]; };
layout(std430, binding = 8) buffer CloudRotations { float cloud_rotations[]; };

uniform mat4 u_viewproj;
uniform float u_cloud_size;
uniform vec2 u_viewport;

out vec3 v_cloud_color;
out float v_rotation;

void main()
{
    vec4 world_pos = vec4(cloud_positions[gl_VertexID].xyz, 1.0);
    vec4 clip_pos = u_viewproj * world_pos;
    gl_Position = clip_pos;

    // Per-cloud size multiplied by global cloud size
    float size = cloud_sizes[gl_VertexID] * u_cloud_size;
    gl_PointSize = size * u_viewport.y / clip_pos.w;
    gl_PointSize = max(gl_PointSize, 1.0);

    v_cloud_color = cloud_colors[gl_VertexID].rgb;
    v_rotation = cloud_rotations[gl_VertexID];
}
