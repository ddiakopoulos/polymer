#version 330

layout(location = 0) in vec3 inPosition;

uniform mat4 u_modelViewProj = mat4(1.0);

out vec2 v_texcoord0;

void main()
{
    gl_Position = u_modelViewProj * vec4(inPosition, 1);
    vec2 inTexcoord = (inPosition.xy + vec2(1,1)) / 2.0;
    v_texcoord0 = inTexcoord;
}