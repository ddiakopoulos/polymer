#version 450 core

uniform sampler2D s_texColor;

in vec2 v_texcoord0;

out vec4 f_color;

void main()
{
    vec4 r = textureGather(s_texColor, v_texcoord0, 0);
    vec4 g = textureGather(s_texColor, v_texcoord0, 1);
    vec4 b = textureGather(s_texColor, v_texcoord0, 2);
    vec4 a = textureGather(s_texColor, v_texcoord0, 3);

    vec4 sum = vec4(0);
    for (int i=0;i<4;++i)
    {
        sum += vec4(r[i], g[i], b[i], a[i]);  
    }
    sum /= 4.0f;

    f_color = sum;
}