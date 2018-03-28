#version 330

uniform vec4 u_offset[16];

uniform sampler2D s_texColor;

in vec2 v_texcoord0;

out vec4 f_color;

vec3 luma(vec3 rgb)
{
    float yy = dot(vec3(0.2126729, 0.7151522, 0.0721750), rgb);
    return vec3(yy, yy, yy);
}

vec4 luma(vec4 rgba)
{
    return vec4(luma(rgba.xyz), rgba.w);
}

void main()
{
    float delta = 0.0001;

    vec3 rgb0 = texture(s_texColor, v_texcoord0 + u_offset[0].xy).rgb;
    vec3 rgb1 = texture(s_texColor, v_texcoord0 + u_offset[1].xy).rgb;
    vec3 rgb2 = texture(s_texColor, v_texcoord0 + u_offset[2].xy).rgb;
    vec3 rgb3 = texture(s_texColor, v_texcoord0 + u_offset[3].xy).rgb;
    vec3 rgb4 = texture(s_texColor, v_texcoord0 + u_offset[4].xy).rgb;
    vec3 rgb5 = texture(s_texColor, v_texcoord0 + u_offset[5].xy).rgb;
    vec3 rgb6 = texture(s_texColor, v_texcoord0 + u_offset[6].xy).rgb;
    vec3 rgb7 = texture(s_texColor, v_texcoord0 + u_offset[7].xy).rgb;
    vec3 rgb8 = texture(s_texColor, v_texcoord0 + u_offset[8].xy).rgb;

    float avg = luma(rgb0).x
              + luma(rgb1).x
              + luma(rgb2).x
              + luma(rgb3).x
              + luma(rgb4).x
              + luma(rgb5).x
              + luma(rgb6).x
              + luma(rgb7).x
              + luma(rgb8).x;

    avg *= 1.0/9.0;

    f_color = vec4(avg, avg, avg, avg);
}