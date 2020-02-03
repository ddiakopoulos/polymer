#version 330

uniform float u_time;
uniform sampler2D s_particle_tex;

in vec3 v_position;
in vec2 v_texcoord;
in vec4 v_color;

out vec4 f_color;

void main() 
{
    vec4 tex = texture2D(s_particle_tex, v_texcoord);
    f_color = vec4(v_color.xyz, tex.a);

    //f_color = v_color;
}
