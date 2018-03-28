#version 330

uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;
uniform mat4 u_viewProj;
uniform vec3 u_eye;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 3) in vec2 inTexcoord;

out vec3 v_position, v_normal;
out vec2 v_texcoord;
out vec4 v_clipspace;

// refraction indices (http://en.wikipedia.org/wiki/Refractive_index Reflectivity)
// http://en.wikipedia.org/wiki/Schlick%27s_approximation
const float airIdx = 0.50;
const float glassIdx = 1.51714;
const float eta = airIdx / glassIdx;
const float R0 = ((airIdx - glassIdx) * (airIdx - glassIdx)) / ((airIdx + glassIdx) * (airIdx + glassIdx));
out float v_fresnel;

void main()
{
    v_position = (u_modelMatrix * vec4(inPosition, 1)).xyz;
    v_normal = normalize((u_modelMatrixIT * vec4(inNormal, 0)).xyz);
    v_texcoord = inTexcoord;

    // Schlick approximation
    vec3 incident = normalize(v_position - u_eye);

    v_fresnel = R0 + (1.0 - R0) * pow((1.0 - dot(incident, v_normal)), 1.0);

    gl_Position = u_viewProj * vec4(v_position, 1);
}