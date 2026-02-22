#version 450

in vec2 v_texcoord;
out vec4 frag_color;

uniform sampler2D u_input_texture;
uniform sampler2D u_draw_texture;
uniform vec2 u_resolution;
uniform int u_show_surface;

void main()
{
    vec4 rc = texture(u_input_texture, v_texcoord);
    vec4 d = texture(u_draw_texture, v_texcoord);
    frag_color = vec4(d.a > 0.0 && u_show_surface == 1 ? d.rgb : rc.rgb, 1.0);
}
