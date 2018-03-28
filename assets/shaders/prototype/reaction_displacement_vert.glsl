#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 3) in vec2 inUV;

uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;
uniform mat4 u_viewProj;

uniform sampler2D u_displacementTex;

out float dOffset;
out vec3 position;
out vec3 normal;

void main() 
{
    float offset = texture(u_displacementTex, inUV).r;
    dOffset = offset;

 	vec3 pos = vec3(inPosition.xy, 2.0 * offset);

    vec4 worldPos = u_modelMatrix * vec4(pos, 1);
    gl_Position = u_viewProj * worldPos;
    position = worldPos.xyz;
    normal = normalize((u_modelMatrixIT * vec4(inNormal, 1)).xyz);
}