#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_previous_position;
layout(location = 2) in vec3 in_color;
layout(location = 3) in vec2 in_grid_uv;

uniform mat4 u_current_mvp;
uniform mat4 u_previous_mvp;

out vec4 v_current_clip;
out vec4 v_previous_clip;

void main()
{
    v_current_clip = u_current_mvp * vec4(in_position, 1.0);
    v_previous_clip = u_previous_mvp * vec4(in_previous_position, 1.0);
    gl_Position = v_current_clip;
}
