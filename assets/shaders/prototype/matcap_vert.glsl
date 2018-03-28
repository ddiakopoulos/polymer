#version 330

uniform mat4 u_modelMatrix;
uniform mat4 u_viewProj;
uniform mat4 u_modelViewMatrix;
uniform mat3 u_modelMatrixIT;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

out vec3 cameraIncident;
out vec3 cameraNormal;

void main()
{
    // Incident ray in camera space
    cameraIncident = normalize(vec3(u_modelViewMatrix * vec4(inPosition, 1)));
    
    // Normal in camera space
    cameraNormal = normalize(u_modelViewMatrix * vec4(inNormal, 0)).xyz;

    vec4 worldPos = u_modelMatrix * vec4(inPosition, 1);
    gl_Position = u_viewProj * worldPos;
}