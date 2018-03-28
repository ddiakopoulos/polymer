#version 330 core

uniform vec3 u_lightWorldPosition;

in vec3 v_lightWorldPos;

out float f_color;

void main()
{
	f_color = distance(v_lightWorldPos, u_lightWorldPosition);
}