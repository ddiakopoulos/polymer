#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;

uniform mat4 u_viewMatrix;
uniform mat4 u_viewProjMatrix;
uniform vec3 u_eyePos; // world space

uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;

out vec3 v_position;
out vec3 v_normal;
out vec2 v_texcoord; 
out vec3 v_tangent;
out vec3 v_bitangent; 

void main()
{
    vec4 worldPos = u_modelMatrix * vec4(inPosition, 1);
    gl_Position = u_viewProjMatrix * worldPos;
    v_position = worldPos.xyz;
    v_normal = normalize((u_modelMatrixIT * vec4(inNormal,0)).xyz);
    v_texcoord = inTexCoord.st; // * vec2(1.0, -1.0);
    v_tangent = inTangent;
    v_bitangent = inBitangent;
}