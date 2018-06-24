#include "renderer_common.glsl"

layout(location = 0) in vec2 inVertexAttr;

void main(void)
{	
	gl_Position = vec4(inVertexAttr, 0.0, 1.0);
}