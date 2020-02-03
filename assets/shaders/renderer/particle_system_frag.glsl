#version 330

uniform float u_time;
uniform sampler2D s_particle_tex;
uniform float u_use_alpha_mask = 0;

in vec3 v_position;
in vec2 v_texcoord;
in vec4 v_color;

out vec4 f_color;

void main() 
{
    if (u_use_alpha_mask >= 1) 
    {
        float falloff = texture2D(s_particle_tex, v_texcoord).a;
        f_color = vec4(v_color.xyz, falloff * v_color.a);
    }
    else
    {
        f_color = vec4(v_color);
    }
}
