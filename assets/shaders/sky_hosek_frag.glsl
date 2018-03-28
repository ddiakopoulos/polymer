#version 330

out vec4 out_color;

in vec3 direction;

uniform vec3 A, B, C, D, E, F, G, H, I, Z;
uniform vec3 SunDirection;

// ArHosekSkyModel_GetRadianceInternal
vec3 HosekWilkie(float cos_theta, float gamma, float cos_gamma)
{
	vec3 chi = (1 + cos_gamma * cos_gamma) / pow(1 + H * H - 2 * cos_gamma * H, vec3(1.5));
    return (1 + A * exp(B / (cos_theta + 0.01))) * (C + D * exp(E * gamma) + F * (cos_gamma * cos_gamma) + G * chi + I * sqrt(cos_theta));
}

void main()
{
	vec3 V = normalize(direction);
	float cos_theta = clamp(V.y, 0, 1);
	float cos_gamma = clamp(dot(V, SunDirection), 0, 1);
	float gamma_ = acos(cos_gamma);

	vec3 R = Z * HosekWilkie(cos_theta, gamma_, cos_gamma);
    out_color = vec4(R, 1);
}