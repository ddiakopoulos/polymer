#version 450

in vec2 v_texcoord;
out vec2 frag_seed;

uniform sampler2D u_surface_texture;

void main()
{
    float alpha = texelFetch(u_surface_texture, ivec2(gl_FragCoord.xy), 0).a;
    frag_seed = v_texcoord * ceil(alpha);
}
