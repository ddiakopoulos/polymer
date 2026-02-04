#version 450

in vec3 g_color;
in vec3 g_position;
in float g_fade;
in vec3 g_barycentric;
in vec3 g_edge_mask;

uniform float u_line_width;
uniform float u_glow_intensity;
uniform float u_near;
uniform float u_far;
uniform float u_distance_fade_start; // normalized [0,1]
uniform float u_distance_fade_end;   // normalized [0,1]
uniform float u_line_width_boost;

out vec4 f_color;

float edge_factor(float line_width)
{
    // Derivative-based line AA (stable for subpixel lines)
    vec3 bc = mix(vec3(1.0), g_barycentric, g_edge_mask);
    float edge = min(min(bc.x, bc.y), bc.z);
    float width = fwidth(edge) * line_width;
    return smoothstep(0.0, width, edge);
}

float linearize_depth(float depth)
{
    float z = depth * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
}

void main()
{
    float linear_depth = linearize_depth(gl_FragCoord.z);
    float depth_norm = clamp(linear_depth / u_far, 0.0, 1.0);
    float dist_fade = 1.0 - smoothstep(u_distance_fade_start, u_distance_fade_end, depth_norm);
    float width_scale = mix(1.0, u_line_width_boost, smoothstep(u_distance_fade_start, u_distance_fade_end, depth_norm));

    float edge = 1.0 - edge_factor(u_line_width * width_scale);

    // Holographic glow effect - color from vertex, intensity from glow parameter
    vec3 line_color = g_color * u_glow_intensity;

    // Height-based intensity boost
    float height_factor = clamp(g_position.y * 0.5 + 0.5, 0.0, 1.0);
    line_color *= (0.8 + 0.4 * height_factor);

    // Alpha combines edge detection, fade, and a base glow
    float alpha = edge * g_fade * dist_fade;

    // Discard fragments with very low alpha for performance
    if (alpha < 0.01) discard;

    f_color = vec4(line_color, alpha);
}
