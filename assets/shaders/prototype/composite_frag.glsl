#version 330

uniform sampler2D s_texColor;
uniform sampler2D s_texGlow;

in vec2 texCoord;
out vec4 f_color;

void main()
{
    f_color = texture(s_texColor, texCoord) + texture(s_texGlow, texCoord);
}