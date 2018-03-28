#version 330

uniform sampler2D u_matcapTexture;

in vec3 cameraIncident;
in vec3 cameraNormal;

out vec4 f_color;

void main()
{
	vec3 r = reflect(cameraIncident, cameraNormal);
    r.z += 1.0;
    
	float m = 0.5 * inversesqrt(dot(r, r)); // 0.5 / length(r)
	vec2 uv = r.xy * m + 0.5;

	f_color = vec4(texture(u_matcapTexture, uv).rgb, 1.0);
}