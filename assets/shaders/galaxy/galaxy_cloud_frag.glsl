#version 450

in vec3 v_cloud_color;
in float v_rotation;

uniform sampler2D s_cloud_texture;
uniform float u_cloud_opacity;

out vec4 f_color;

void main()
{
    // Circular falloff to hide the square point sprite boundary
    vec2 center = gl_PointCoord * 2.0 - 1.0;
    float dist = length(center);
    if (dist > 1.0) discard;
    float circle_fade = smoothstep(1.0, 0.4, dist);

    // Rotate UV around center
    vec2 uv = gl_PointCoord - 0.5;
    float c = cos(v_rotation);
    float s = sin(v_rotation);
    vec2 rotated_uv = vec2(uv.x * c - uv.y * s, uv.x * s + uv.y * c) + 0.5;

    // Discard fragments where rotated UV falls outside texture bounds
    if (rotated_uv.x < 0.0 || rotated_uv.x > 1.0 || rotated_uv.y < 0.0 || rotated_uv.y > 1.0) discard;

    float tex_alpha = texture(s_cloud_texture, rotated_uv).a;
    float alpha = tex_alpha * u_cloud_opacity * circle_fade;

    if (alpha < 0.001) discard;

    f_color = vec4(v_cloud_color, alpha);
}
