#version 330

uniform mat4 u_modelMatrix;
uniform mat4 u_viewMat;
uniform mat4 u_viewProjMat;

layout(location = 0) in vec4 in_pos_size;
layout(location = 1) in vec2 in_texcoord;

out vec3 v_position;
out vec2 v_texcoord;

void main() 
{
    mat4 invView = inverse(u_viewMat);
    vec3 qxdir = (invView * vec4(1, 0, 0, 0)).xyz;
    vec3 qydir = (invView * vec4(0, 1, 0, 0)).xyz;

    vec4 position = vec4(in_pos_size.xyz + qxdir * ((in_texcoord.x*2-1)*in_pos_size.w) + qydir * ((in_texcoord.y*2-1)*in_pos_size.w), 1);
    v_position = (u_viewMat * vec4((u_modelMatrix * position).xyz, 1)).xyz;
    v_texcoord = in_texcoord;
    gl_Position = u_viewProjMat * u_modelMatrix * position;
}