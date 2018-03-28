#version 330

uniform sampler2D s_emissiveTex;
uniform sampler2D s_diffuseTex;
uniform float u_emissivePower;

in vec3 v_position;
in vec3 v_normal;
in vec2 v_texcoord;

out vec4 f_color;

void main()
{
    float samp = texture(s_emissiveTex, v_texcoord).r;

    f_color = vec4(0, 0, 0, 1);

    if (samp < 0.50)
    {
        f_color = vec4(1 * u_emissivePower, 1, 1, 1);
    }
}