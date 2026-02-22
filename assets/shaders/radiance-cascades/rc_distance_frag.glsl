#version 450

in vec2 v_texcoord;
out float frag_distance;

uniform sampler2D u_jfa_texture;
uniform vec2 u_resolution;

void main()
{
    ivec2 texel = ivec2(v_texcoord * u_resolution);
    vec2 nearest_seed = texelFetch(u_jfa_texture, texel, 0).xy;
    float dist = clamp(distance(v_texcoord, nearest_seed), 0.0, 1.0);
    frag_distance = dist;
}
