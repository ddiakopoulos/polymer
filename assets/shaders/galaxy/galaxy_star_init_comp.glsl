#version 450

layout(local_size_x = 256) in;

layout(std430, binding = 0) buffer StarPositions { vec4 star_positions[]; };
layout(std430, binding = 1) buffer StarOriginals { vec4 star_originals[]; };
layout(std430, binding = 2) buffer StarVelocities { vec4 star_velocities[]; };
layout(std430, binding = 3) buffer StarDensity { float star_density[]; };

uniform float u_galaxy_radius;
uniform float u_galaxy_thickness;
uniform float u_spiral_tightness;
uniform float u_arm_count;
uniform float u_arm_width;
uniform float u_randomness;
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

    // Each property uses a large offset to ensure fully independent random values
    uint base = idx;

    // Distance from center (square root for even area distribution)
    float radius = pow(hash(base), 0.5) * u_galaxy_radius;
    float normalized_radius = radius / u_galaxy_radius;

    // Choose spiral arm
    float arm_index = floor(hash(base + 1000000u) * u_arm_count);
    float arm_angle = arm_index * 6.28318 / u_arm_count;

    // Spiral angle based on distance (logarithmic spiral)
    float spiral_angle = normalized_radius * u_spiral_tightness * 6.28318;

    // Add randomness for natural appearance
    float angle_offset = (hash(base + 2000000u) - 0.5) * u_randomness;
    float radius_offset = (hash(base + 3000000u) - 0.5) * u_arm_width;

    // Final angle and radius
    float angle = arm_angle + spiral_angle + angle_offset;
    float offset_radius = radius + radius_offset;

    // Convert to Cartesian coordinates
    float x = cos(angle) * offset_radius;
    float z = sin(angle) * offset_radius;

    // Vertical position: thicker at center, thinner at edges
    float thickness_factor = (1.0 - normalized_radius) + 0.2;
    float y = (hash(base + 4000000u) - 0.5) * u_galaxy_thickness * thickness_factor;

    vec3 position = vec3(x, y, z);

    star_positions[idx] = vec4(position, 1.0);
    star_originals[idx] = vec4(position, 1.0);

    // Orbital velocity (faster closer to center)
    float orbital_speed = 1.0 / (offset_radius + 0.5) * 5.0;
    float vx = -sin(angle) * orbital_speed;
    float vz = cos(angle) * orbital_speed;
    star_velocities[idx] = vec4(vx, 0.0, vz, 0.0);

    // Density factor for coloring (0 = dense/center, 1 = sparse/edge)
    float radial_sparsity = abs(radius_offset) / (u_arm_width * 0.5 + 0.01);
    float angular_sparsity = abs(angle_offset) / (u_randomness * 0.5 + 0.01);
    float sparsity = clamp((radial_sparsity + angular_sparsity) * 0.5, 0.0, 1.0);
    star_density[idx] = sparsity;
}
