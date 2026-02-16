#version 450

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_color;

uniform mat4 u_viewproj;
uniform vec2 u_viewport;

out vec3 v_color;

void main()
{
    vec4 clip_pos = u_viewproj * vec4(a_position, 1.0);
    gl_Position = clip_pos;

    // Small fixed-ish size with some attenuation
    gl_PointSize = max(0.3 * u_viewport.y / clip_pos.w, 1.0);

    v_color = a_color;
}
