#version 330

uniform sampler2D s_billboard;

in vec3 v_position, v_normal;
in vec2 v_texcoord;

out vec4 f_color;

in vec4 v_clipspace;

const float borderWidth = 0.025f;
const float maxX = 1.0 - borderWidth;
const float minX = borderWidth;
const float maxY = maxX / 1.0;
const float minY = minX / 1.0;

void main()
{
    vec2 texcoord = v_clipspace.xy / v_clipspace.w; // perspective divide = translate to ndc to [-1, 1]
    texcoord = (texcoord + vec2(1)) * 0.5; // remap -1, 1 ndc to uv coords [0,1]

    if (v_texcoord.x < maxX && v_texcoord.x > minX && v_texcoord.y < maxY && v_texcoord.y > minY) 
    {
        f_color = texture(s_billboard, texcoord);
    }
    else 
    {
        f_color = vec4(0, 0, 0, 1);
    }
}
