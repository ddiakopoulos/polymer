#version 330

uniform mat4 u_modelMatrix;
uniform mat4 u_inverseViewMatrix;
uniform mat4 u_viewProjMat;


layout(location = 0) in vec4 in_pos_size;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_texcoord;

out vec3 v_position;
out vec2 v_texcoord;
out vec3 v_color;

void main() 
{
    vec3 qxdir = (u_inverseViewMatrix * vec4(1, 0, 0, 0)).xyz;
    vec3 qydir = (u_inverseViewMatrix * vec4(0, 1, 0, 0)).xyz;

    float sizeX = (in_texcoord.x * 2 - 1) * in_pos_size.w;
    float sizeY = (in_texcoord.y * 2 - 1) * in_pos_size.w;

    vec4 position = vec4(in_pos_size.xyz + qxdir * (sizeX) + qydir * (sizeY), 1);
    v_position = (u_inverseViewMatrix * vec4((u_modelMatrix * position).xyz, 1)).xyz;
    v_texcoord = in_texcoord;
    v_color = in_color.rgb;
    gl_Position = u_viewProjMat * u_modelMatrix * position;
}
