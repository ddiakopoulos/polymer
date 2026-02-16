#version 450

uniform sampler2D s_source;
uniform vec2 u_direction;
uniform int u_kernel_radius;
uniform float u_weights[12];

in vec2 v_texcoord;
out vec4 f_color;

void main()
{
    vec3 result = texture(s_source, v_texcoord).rgb * u_weights[0];

    for (int i = 1; i <= u_kernel_radius; ++i)
    {
        vec2 offset = u_direction * float(i);
        result += texture(s_source, v_texcoord + offset).rgb * u_weights[i];
        result += texture(s_source, v_texcoord - offset).rgb * u_weights[i];
    }

    f_color = vec4(result, 1.0);
}
