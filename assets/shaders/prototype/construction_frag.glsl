#include "renderer_common.glsl"

in vec3 v_world_position;
in vec3 v_view_space_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_tangent;
in vec3 v_bitangent;

out vec4 f_color;

#define NUM_LINES 20.0

vec2 rotate_coordinate(vec2 uv, float rads) 
{
    uv *= mat2(cos(rads), sin(rads), -sin(rads), cos(rads));
    return uv;
}

void main()
{
    float rows = NUM_LINES * 0.5;
    float curThickness = 0.25;
    float curRotation = 0.707;

    // get original coordinate, translate & rotate
    vec2 uv = v_texcoord;
    uv = rotate_coordinate(uv, curRotation);

    // create grid coords
    vec2 uvRepeat = fract(uv * rows);       

    // adaptive antialiasing, draw
    float aa = resolution.y * 0.00003;     
    float col = clamp(smoothstep(curThickness - aa, curThickness + aa, length(uvRepeat.y - 0.5)), 0, 1);       

    float alpha = col > 0.f ? .33 : 0.f;
    f_color = vec4(vec3(0.913, 0.360, 0.125), alpha);
}