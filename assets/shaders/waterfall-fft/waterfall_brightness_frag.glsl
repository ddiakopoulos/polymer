#version 450

uniform sampler2D s_hdr_color;
uniform float u_threshold;

in vec2 v_texcoord;
out vec4 f_color;

void main()
{
    vec3 color = texture(s_hdr_color, v_texcoord).rgb;

    // Calculate luminance
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));

    // Soft-knee threshold
    float soft_threshold = u_threshold * 0.9;
    float contribution = max(0.0, luminance - soft_threshold);
    contribution = contribution / (contribution + 1.0);

    // Extract bright pixels
    float brightness = max(0.0, luminance - u_threshold);
    float weight = brightness / max(luminance, 0.0001);

    f_color = vec4(color * weight * contribution * 2.0, 1.0);
}
