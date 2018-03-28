#include "renderer_common.glsl"

uniform float u_cascadeNear[NUM_CASCADES];
uniform float u_cascadeFar[NUM_CASCADES];

in float g_layer;
in vec3 vs_position;

void main() 
{
	// opengl takes care of this already
}