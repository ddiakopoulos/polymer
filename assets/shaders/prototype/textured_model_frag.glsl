#version 450 core

const int NUM_LIGHTS = 2;

in vec3 v_position; 
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_tangent;
in vec3 v_bitangent;

struct PointLight
{
    vec3 position;
    vec3 diffuseColor;
    vec3 specularColor;
};

struct MaterialProperties
{
    vec3 diffuseIntensity;  // reflectance for diffuse light
    vec3 ambientIntensity;  // reflectance for ambient light
    vec3 specularIntensity; // reflectance for specular light
    float specularPower;    // Specular reflectance exponent
}; 

struct RimLight
{
    int enable;
    vec3 color;
    float power;
};

uniform PointLight u_pointLights[NUM_LIGHTS];
uniform RimLight u_rimLight;

uniform MaterialProperties u_material;

uniform vec3 u_ambientLight;

uniform sampler2D u_diffuseTex;
uniform sampler2D u_normalTex;
uniform sampler2D u_specularTex;
uniform sampler2D u_emissiveTex;

uniform int u_enableDiffuseTex = 1;
uniform int u_enableNormalTex = 0;
uniform int u_enableSpecularTex = 0;
uniform int u_enableGlossTex = 0;
uniform int u_enableEmissiveTex = 0;

uniform mat4 u_viewMatrix;
uniform mat4 u_viewProjMatrix;
uniform vec3 u_eyePos; // world space

uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;

//uniform int u_enableGlossTex = 0;
//uniform int u_enableAmbientOcclusionTex = 0;

out vec4 f_color;

vec3 compute_rimlight(vec3 n, vec3 v)
{
    float f = 1.0f - dot(n, v);
    f = smoothstep(0.0f, 1.0f, f);
    f = pow(f, u_rimLight.power);
    return f * u_rimLight.color; 
}

vec3 sample_normal_texture(sampler2D normalTexture, vec2 texCoord, vec3 tangent, vec3 bitangent, vec3 normal)
{
    vec3 tSpaceNorm = texture(normalTexture, texCoord).xyz * 2 - 1;
    return normalize(normalize(tangent) * tSpaceNorm.x + normalize(bitangent) * tSpaceNorm.y + normalize(normal) * tSpaceNorm.z);
}

void main()
{
    vec3 eyeDir = normalize(u_eyePos - v_position);

    vec4 ambientLight = vec4(u_material.ambientIntensity * u_ambientLight, 1.0);
    vec4 diffuseLight = vec4(0.0);
    vec4 specularLight = vec4(0.0);
    vec4 emissiveLight = vec4(0.0);

    vec4 diffuseSample = vec4(0.5);
    if (u_enableDiffuseTex == 1)
    {
        diffuseSample = texture(u_diffuseTex, v_texcoord);
    }

    vec3 normalSample = v_normal;
    if (u_enableNormalTex == 1)
    {
        normalSample = sample_normal_texture(u_normalTex, v_texcoord, v_tangent, v_bitangent, v_normal);
    }
    
    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        vec3 lightDir = normalize(u_pointLights[i].position - v_position);
        diffuseLight += vec4(u_material.diffuseIntensity * u_pointLights[i].diffuseColor * max(dot(normalSample, lightDir), 0), 1);

        vec3 halfDir = normalize(lightDir + eyeDir);
        specularLight += vec4(u_material.specularIntensity * u_pointLights[i].specularColor * pow(max(dot(normalSample, halfDir), 0), u_material.specularPower), 1);
    }

    vec4 specularSample = vec4(0.0);
    if (u_enableSpecularTex == 1)
    {
        specularSample = texture(u_specularTex, v_texcoord);
    }

    if (u_enableEmissiveTex == 1)
    {
        emissiveLight = texture(u_emissiveTex, v_texcoord);
    }

    if (u_rimLight.enable == 1)
    {
        diffuseLight += vec4(compute_rimlight(normalSample, eyeDir), 1);
    }

    f_color = (ambientLight * diffuseSample) + (diffuseLight * diffuseSample) + (specularLight * specularSample) + (emissiveLight);
}