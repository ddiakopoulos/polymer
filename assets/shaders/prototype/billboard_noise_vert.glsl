#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inVertexColor;
layout(location = 3) in vec2 inTexcoord;

uniform vec3 u_eyePos;
uniform mat4 u_viewProjMatrix;
uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;

out vec3 v_position;
out vec3 v_normal;
out vec2 v_texcoord0;
out vec2 v_texcoord1;

uniform float u_time;
uniform vec2 u_resolution;
uniform vec2 u_invResolution;
uniform sampler2D s_mainTex;
uniform sampler2D s_noiseTex;
uniform vec2 u_intensity;
uniform vec2 u_scroll;

void main()
{
    vec4 worldPos = u_modelMatrix * vec4(inPosition, 1);
    v_position = worldPos.xyz;
    v_normal = normalize((u_modelMatrixIT * vec4(inNormal, 0)).xyz);
    v_texcoord0 = inTexcoord * vec2(1, 1);
    v_texcoord1 = inTexcoord * vec2(1, 1);
    v_texcoord1 += vec2(u_time) * u_scroll;
    gl_Position = u_viewProjMatrix * worldPos;
}

// (tex.xy * name##_ST.xy + name##_ST.zw)