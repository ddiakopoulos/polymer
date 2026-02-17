#version 450

in vec2 v_texcoord;
out vec4 frag_color;

uniform sampler2D u_accumulation_tex;
uniform float u_exposure;

void main()
{
    vec4 accum = texture(u_accumulation_tex, v_texcoord);

    if (accum.a < 1.0)
    {
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 color = accum.rgb / accum.a;
    color *= u_exposure;

    frag_color = vec4(color, 1.0);
}
