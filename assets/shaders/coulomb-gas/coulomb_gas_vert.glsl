#version 450

layout(location = 0) in vec2 a_quad_pos;
layout(location = 1) in vec2 a_inst_pos;

uniform float u_sim_to_clip;
uniform float u_particle_size_px;
uniform float u_canvas_w;
uniform float u_canvas_h;

void main()
{
    vec2 base = a_inst_pos * u_sim_to_clip;
    vec2 off_clip = vec2(
        a_quad_pos.x * u_particle_size_px * 2.0 / u_canvas_w,
        a_quad_pos.y * u_particle_size_px * 2.0 / u_canvas_h
    );
    gl_Position = vec4(base + off_clip, 0.0, 1.0);
}
