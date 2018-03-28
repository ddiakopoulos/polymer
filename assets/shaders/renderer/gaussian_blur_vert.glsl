#version 330

layout(location = 0) in vec3 inPosition;

uniform mat4 u_modelViewProj;

out vec2 v_texcoord;

void main()
{
    gl_Position = u_modelViewProj * vec4(inPosition, 1);
    vec2 texcoord = (inPosition.xy + vec2(1,1)) / 2.0;
    v_texcoord = texcoord;
}