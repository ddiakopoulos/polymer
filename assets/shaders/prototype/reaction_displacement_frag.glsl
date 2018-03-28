#version 330

uniform vec3 u_emissive;
uniform vec3 u_diffuse;
uniform vec3 u_eye;
uniform mat4 u_modelMatrixIT;

vec3 lightPosition = vec3(0, 10, 0);
vec3 lightColor = vec3(1, 1, 1);

in vec3 position;
in vec3 normal;
in float dOffset;

out vec4 f_color;

void main(void) 
{
    vec3 p0 = dFdx(position);
    vec3 p1 = dFdy(position);
    vec3 n =  normalize(cross(p0, p1));

    vec3 eyeDir = normalize(u_eye - position);
    vec3 light = u_emissive;

    vec3 lightDir = normalize(lightPosition - position);
    light += lightColor * u_diffuse * max(dot(n, lightDir), 0);

    vec3 halfDir = normalize(lightDir + eyeDir);
    light += lightColor * u_diffuse * pow(max(dot(n, halfDir), 0), 256);

    f_color = vec4(light, 1);
}