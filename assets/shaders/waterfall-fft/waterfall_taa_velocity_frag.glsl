#version 450

in vec4 v_current_clip;
in vec4 v_previous_clip;

out vec2 f_velocity;

void main()
{
    vec2 current_ndc = v_current_clip.xy / v_current_clip.w;
    vec2 previous_ndc = v_previous_clip.xy / v_previous_clip.w;
    f_velocity = 0.5 * (current_ndc - previous_ndc);
}
