#include "renderer_common.glsl"

in vec3 v_world_position;
in vec3 v_view_space_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_tangent;
in vec3 v_bitangent;

out vec4 f_color;

void main()
{   
    f_color = vec4(1, 1, 1, 1);
}
