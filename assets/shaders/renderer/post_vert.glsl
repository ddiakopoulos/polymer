#version 450 core

layout(location = 0) in vec3 inPosition;

uniform mat4 u_modelViewProj = mat4(1.0);

out gl_PerVertex
{
	vec4 gl_Position;
};

out vec2 v_texcoord0;

void main()
{
	vec2 inTexcoord = (inPosition.xy + vec2(1,1)) / 2.0;
    gl_Position = u_modelViewProj * vec4(inPosition, 1);
    v_texcoord0 = inTexcoord;
}