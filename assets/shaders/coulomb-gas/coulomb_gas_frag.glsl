#version 450

layout(location = 0) out vec4 frag_color;

uniform float u_alpha;
uniform float u_is_dark;

void main()
{
    float c = u_is_dark > 0.5 ? 1.0 : 0.0;
    frag_color = vec4(c, c, c, u_alpha);
}
