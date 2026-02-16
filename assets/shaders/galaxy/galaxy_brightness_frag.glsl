#version 450

uniform sampler2D s_hdr_color;
uniform float u_threshold;

in vec2 v_texcoord;
out vec4 f_color;

void main()
{
    vec3 color = texture(s_hdr_color, v_texcoord).rgb;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));

    // Simple soft-knee extraction (no dual-gate)
    float knee = u_threshold * 0.7;
    float soft = luminance - (u_threshold - knee);
    float contribution = clamp(soft / (2.0 * knee + 0.0001), 0.0, 1.0);
    contribution *= contribution; // Smooth ramp

    f_color = vec4(color * contribution, 1.0);
}
