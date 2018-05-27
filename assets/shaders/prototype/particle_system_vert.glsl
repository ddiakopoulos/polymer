#version 330

uniform mat4 u_modelMatrix;
uniform mat4 u_inverseViewMatrix;
uniform mat4 u_viewProjMat;

struct instance_buffer
{
	vec4 pos_size;
	vec4 color;
};

layout(location = 0) in instance_buffer in_instance_buffer;
layout(location = 1) in vec2 in_texcoord;

out vec3 v_position;
out vec2 v_texcoord;
out vec3 v_color;

void main() 
{
    vec3 qxdir = (u_inverseViewMatrix * vec4(1, 0, 0, 0)).xyz;
    vec3 qydir = (u_inverseViewMatrix * vec4(0, 1, 0, 0)).xyz;

    float sizeX = (in_texcoord.x * 2 - 1) * in_instance_buffer.pos_size.w;
    float sizeY = (in_texcoord.y * 2 - 1) * in_instance_buffer.pos_size.w;

    vec4 position = vec4(in_instance_buffer.pos_size.xyz + qxdir * (sizeX) + qydir * (sizeY), 1);
    v_position = (u_inverseViewMatrix * vec4((u_modelMatrix * position).xyz, 1)).xyz;
    v_texcoord = in_texcoord;
    v_color = in_instance_buffer.color.rgb;
    gl_Position = u_viewProjMat * u_modelMatrix * position;
}
