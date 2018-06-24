#include "renderer_common.glsl"

out vec4 f_color;

void main()
{
    f_color = vec4(1); // no-op since hardware is writing depth buffer for us
}