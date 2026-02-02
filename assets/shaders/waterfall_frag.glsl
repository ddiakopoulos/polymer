#version 450

in vec3 v_color;
in vec3 v_position;
in float v_fade;

out vec4 f_color;

void main()
{
    // Simple height-based shading for depth cues
    float height_factor = clamp(v_position.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 shaded = v_color * (0.6 + 0.4 * height_factor);
    f_color = vec4(shaded, v_fade);
}
