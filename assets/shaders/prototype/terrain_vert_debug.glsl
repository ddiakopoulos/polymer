#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 3) in vec2 uv;

uniform mat4 u_model;
uniform mat4 u_modelIT;
uniform mat4 u_viewProj;

out vec3 worldPos;
out vec3 norm;
out vec3 p;

void main() 
{
	// Pass world-space quantities to frag shader
	worldPos = (u_model * vec4(position, 1.0)).xyz;
	//norm = (u_modelIT * vec4(normal, 0.0)).xyz;

	// Output clip-space quantities for OpenGL
    gl_Position = u_viewProj * vec4(worldPos, 1.0);
    
    p = position;
    norm = normal;
}