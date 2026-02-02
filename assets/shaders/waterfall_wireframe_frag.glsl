#version 450

in vec3 g_color;
in vec3 g_position;
in float g_fade;
in vec3 g_barycentric;

uniform float u_line_width;
uniform float u_glow_intensity;

out vec4 f_color;

float edge_factor()
{
    vec3 d = fwidth(g_barycentric);
    vec3 a3 = smoothstep(vec3(0.0), d * u_line_width, g_barycentric);
    return min(min(a3.x, a3.y), a3.z);
}

void main()
{
    float edge = 1.0 - edge_factor();

    // Holographic glow effect - color from vertex, intensity from glow parameter
    vec3 line_color = g_color * u_glow_intensity;

    // Height-based intensity boost
    float height_factor = clamp(g_position.y * 0.5 + 0.5, 0.0, 1.0);
    line_color *= (0.8 + 0.4 * height_factor);

    // Alpha combines edge detection, fade, and a base glow
    float alpha = edge * g_fade;

    // Discard fragments with very low alpha for performance
    if (alpha < 0.01) discard;

    f_color = vec4(line_color, alpha);
}
