#version 450

layout(local_size_x = 256) in;

layout(std430, binding = 4) buffer CloudPositions { vec4 cloud_positions[]; };
layout(std430, binding = 5) buffer CloudOriginals { vec4 cloud_originals[]; };
layout(std430, binding = 6) buffer CloudColors { vec4 cloud_colors[]; };
layout(std430, binding = 7) buffer CloudSizes { float cloud_sizes[]; };
layout(std430, binding = 8) buffer CloudRotations { float cloud_rotations[]; };

uniform float u_galaxy_radius;
uniform float u_galaxy_thickness;
uniform float u_spiral_tightness;
uniform float u_arm_count;
uniform float u_arm_width;
uniform float u_randomness;
uniform vec4 u_cloud_tint; // rgb = tint color, w unused
uniform int u_count;

uint pcg_hash(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float hash(uint v)
{
    return float(pcg_hash(v)) / 4294967295.0;
}

void main()
{
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_count) return;

    // Offset base by 10M to avoid overlap with star seed space
    uint base = idx + 10000000u;

    // Distance from center (power = 0.7 for more even distribution)
    float radius = pow(hash(base), 0.7) * u_galaxy_radius;
    float normalized_radius = radius / u_galaxy_radius;

    // Choose spiral arm
    float arm_index = floor(hash(base + 1000000u) * u_arm_count);
    float arm_angle = arm_index * 6.28318 / u_arm_count;

    // Spiral angle
    float spiral_angle = normalized_radius * u_spiral_tightness * 6.28318;

    // Randomness
    float angle_offset = (hash(base + 2000000u) - 0.5) * u_randomness;
    float radius_offset = (hash(base + 3000000u) - 0.5) * u_arm_width;

    float angle = arm_angle + spiral_angle + angle_offset;
    float offset_radius = radius + radius_offset;

    float x = cos(angle) * offset_radius;
    float z = sin(angle) * offset_radius;

    // Slightly thinner vertical distribution than stars
    float thickness_factor = (1.0 - normalized_radius) + 0.15;
    float y = (hash(base + 4000000u) - 0.5) * u_galaxy_thickness * thickness_factor;

    vec3 position = vec3(x, y, z);

    cloud_positions[idx] = vec4(position, 1.0);
    cloud_originals[idx] = vec4(position, 1.0);

    // Cloud color: tinted and darker towards edges
    vec3 color = u_cloud_tint.rgb * (1.0 - normalized_radius * 0.3);
    cloud_colors[idx] = vec4(color, 1.0);

    // Size variation: larger in denser regions
    float density_factor = 1.0 - normalized_radius * 0.5;
    float size = (hash(base + 5000000u) * 0.5 + 0.7) * density_factor;
    cloud_sizes[idx] = size;

    // Random rotation
    float rotation = hash(base + 6000000u) * 6.28318;
    cloud_rotations[idx] = rotation;
}
