#version 450

uniform sampler2D s_hdr_color;
uniform float u_threshold;
uniform float u_knee;

in vec2 v_texcoord;
out vec4 f_color;

void main()
{
    vec3 color = texture(s_hdr_color, v_texcoord).rgb;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float contribution = smoothstep(u_threshold, u_threshold + u_knee, luminance);
    f_color = vec4(color * contribution, 1.0);
}
