#version 330

in vec3 v_position;
in vec3 v_normal;
in vec2 v_texcoord0;
in vec2 v_texcoord1;

uniform float u_time;
uniform vec2 u_resolution;
uniform vec2 u_invResolution;
uniform sampler2D s_mainTex;
uniform sampler2D s_noiseTex;
uniform vec2 u_intensity;
uniform vec2 u_scroll;

out vec4 f_color;

void main()
{
    vec4 noiseTex = texture(s_noiseTex, v_texcoord1);
    vec2 offset = (noiseTex.r * u_intensity);
    vec2 uvNoise = v_texcoord0 + offset;
    vec4 mainTex = texture(s_mainTex, uvNoise);
    f_color = mainTex;
}