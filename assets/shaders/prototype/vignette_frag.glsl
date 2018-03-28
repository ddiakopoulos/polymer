#version 330

#define REDUCTION_POWER 2.5
#define BOOST 6.0

uniform vec2 u_screenResolution;
uniform float u_noiseAmount;
uniform vec3 u_backgroundColor;
uniform float u_time = 1.0;

out vec4 f_color;

float random(vec3 scale, float seed) { return fract(sin(dot(gl_FragCoord.xyz + seed, scale)) * 43758.5453 + seed); }

void main()
{
	vec2 center = u_screenResolution * 0.5;
	float vignette = distance(center, gl_FragCoord.xy) / u_screenResolution.x;
	vignette = BOOST - vignette * REDUCTION_POWER;
	float n = u_noiseAmount * (0.5 - random(vec3(u_time), length(gl_FragCoord)));
	f_color = vec4(u_backgroundColor * vec3(vignette) + vec3(n), 1.0);
}
