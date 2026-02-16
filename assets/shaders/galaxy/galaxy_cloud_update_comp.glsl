#version 450

layout(local_size_x = 256) in;

layout(std430, binding = 4) buffer CloudPositions { vec4 cloud_positions[]; };
layout(std430, binding = 5) buffer CloudOriginals { vec4 cloud_originals[]; };

uniform float u_rotation_speed;
uniform float u_delta_time;
uniform vec4 u_mouse_pos;
uniform float u_mouse_active;
uniform float u_mouse_force;
uniform float u_mouse_radius;
uniform int u_count;

vec3 rotate_xz(vec3 pos, float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return vec3(pos.x * c - pos.z * s, pos.y, pos.x * s + pos.z * c);
}

void main()
{
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_count) return;

    vec3 position = cloud_positions[idx].xyz;
    vec3 original = cloud_originals[idx].xyz;

    // 1. Differential rotation
    float dist_xz = length(vec2(position.x, position.z));
    float rotation_factor = 1.0 / (dist_xz * 0.1 + 1.0);
    float angular_speed = -u_rotation_speed * rotation_factor * u_delta_time;
    position = rotate_xz(position, angular_speed);

    float orig_dist_xz = length(vec2(original.x, original.z));
    float orig_factor = 1.0 / (orig_dist_xz * 0.1 + 1.0);
    float orig_speed = -u_rotation_speed * orig_factor * u_delta_time;
    original = rotate_xz(original, orig_speed);
    cloud_originals[idx] = vec4(original, 1.0);

    // 2. Mouse repulsion
    vec3 to_mouse = u_mouse_pos.xyz - position;
    float dist_to_mouse = length(to_mouse);
    float influence = u_mouse_active * max(1.0 - dist_to_mouse / u_mouse_radius, 0.0);
    vec3 mouse_dir = dist_to_mouse > 0.001 ? normalize(to_mouse) : vec3(0.0);
    position -= mouse_dir * u_mouse_force * influence * u_delta_time;

    // 3. Spring force (weaker than stars for more fluid movement)
    vec3 spring_force = (original - position) * 1.0 * u_delta_time;
    position += spring_force;

    cloud_positions[idx] = vec4(position, 1.0);
}
