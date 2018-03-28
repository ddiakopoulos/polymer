#version 330

const int MAX_SPOT_LIGHTS = 2;
const int MAX_POINT_LIGHTS = 1;

const int POINT_LIGHT = 0;
const int SPOT_LIGHT = 1;

struct DirectionalLight
{
    vec3 direction;
    vec3 color;
};

struct PointLight
{
    vec3 position;
    vec3 color;
    float constantAtten;
    float linearAtten;
    float quadraticAtten;
};

struct SpotLight
{
    vec3 direction;
    vec3 position;
    vec3 color;
    float cutoff;
    float constantAtten;
    float linearAtten;
    float quadraticAtten;
};

in vec3 v_world_position;
in vec2 v_texcoord;
in vec3 v_normal;
in vec4 v_camera_directional_light;
in vec4 v_camera_spot_light[MAX_SPOT_LIGHTS];

out vec4 f_color;

uniform DirectionalLight u_directionalLight;
uniform SpotLight u_spotLights[MAX_SPOT_LIGHTS];
uniform sampler2D s_spotLightShadowMap[MAX_SPOT_LIGHTS];

uniform sampler2D s_directionalShadowMap;

uniform PointLight u_pointLights[MAX_POINT_LIGHTS];
uniform samplerCube s_pointLightCubemap[MAX_POINT_LIGHTS];

uniform float u_shadowMapBias;
uniform vec2 u_shadowMapTexelSize;

uniform vec3 u_eye;

// http://http.developer.nvidia.com/GPUGems/gpugems_ch12.html -- better bias
float calculate_shadow_factor_cube(vec3 toFragment, int i)
{
    float samp = texture(s_pointLightCubemap[i], toFragment).r;
    float dist = length(toFragment) * 0.5f;
    if (dist < samp + u_shadowMapBias) return 1.0;
    else return 0.5;
}

float sample_shadow_map(sampler2D shadowMap, vec2 uv, float compare)
{
    float depth = texture(shadowMap, uv).x;
    if (depth < compare) return 0.5;
    else return 1.0;
}

float shadow_map_linear(sampler2D shadowMap, vec2 coords, float compare, vec2 texelSize)
{
    vec2 pixelPos = coords/texelSize + vec2(0.5);
    vec2 fracPart = fract(pixelPos);
    vec2 startTexel = (pixelPos - fracPart) * texelSize;
    
    float blTexel = sample_shadow_map(shadowMap, startTexel, compare);
    float brTexel = sample_shadow_map(shadowMap, startTexel + vec2(texelSize.x, 0.0), compare);
    float tlTexel = sample_shadow_map(shadowMap, startTexel + vec2(0.0, texelSize.y), compare);
    float trTexel = sample_shadow_map(shadowMap, startTexel + texelSize, compare);
    
    float mixA = mix(blTexel, tlTexel, fracPart.y);
    float mixB = mix(brTexel, trTexel, fracPart.y);
    
    return mix(mixA, mixB, fracPart.x);
}

float shadow_map_pcf(sampler2D shadowMap, vec2 coords, float compare, vec2 texelSize)
{
    const float NUM_SAMPLES = 3.0f;
    const float SAMPLES_START = (NUM_SAMPLES - 1.0f) / 2.0f;
    const float NUM_SAMPLES_SQUARED = NUM_SAMPLES * NUM_SAMPLES;

    float result = 0.0f;
    for (float y = -SAMPLES_START; y <= SAMPLES_START; y += 1.0f)
    {
        for (float x = -SAMPLES_START; x <= SAMPLES_START; x += 1.0f)
        {
            vec2 offset = vec2(x,y) * texelSize;
            result += shadow_map_linear(shadowMap, coords + offset, compare, texelSize);
        }
    }
    return result / NUM_SAMPLES_SQUARED;
}

bool in_range(float val)
{
    return val >= 0.0 && val <= 1.0;
}

float calculate_shadow_factor(vec4 cameraSpacePosition, int i)
{
    vec3 projectedCoords = cameraSpacePosition.xyz / cameraSpacePosition.w;

    vec2 uvCoords;
    uvCoords.x = 0.5 * projectedCoords.x + 0.5;
    uvCoords.y = 0.5 * projectedCoords.y + 0.5;
    float z = (0.5 * projectedCoords.z + 0.5); // self shadow bias
    
    return shadow_map_pcf(s_spotLightShadowMap[i], uvCoords, z - u_shadowMapBias, u_shadowMapTexelSize);
}

vec4 calculate_point_light(vec3 position, vec3 color, float constantAtten, float linearAtten, float quadraticAtten, vec4 lightSpacePos, int i, int baseLightType)
{
    vec3 toLightVector = position - v_world_position;
    float d = length(toLightVector);
    float attenuation = 1.0 / (constantAtten + linearAtten * d + quadraticAtten * d * d);
    float coeff = max(0.0, dot(v_normal, normalize(toLightVector)));
    
    float shadowFactor = 1.0;
    if (baseLightType == SPOT_LIGHT) shadowFactor = calculate_shadow_factor(lightSpacePos, i);
    else if (baseLightType == POINT_LIGHT) shadowFactor = calculate_shadow_factor_cube(-toLightVector, i);

    return vec4(color * attenuation * coeff * shadowFactor, 1.0);
}

vec4 calculate_spot_light(SpotLight sl, vec4 lightSpacePos, int i)
{
    vec3 toLightVector = sl.position - v_world_position;
    float spotFactor = dot(normalize(-toLightVector), sl.direction);
    
    if (spotFactor > sl.cutoff)
    {
        vec4 pointLightComponent = calculate_point_light(sl.position, sl.color, sl.constantAtten, sl.linearAtten, sl.quadraticAtten, lightSpacePos, i, SPOT_LIGHT);
        float fadeFactor = (1.0 - (1.0 - spotFactor) * 1.0 / (1.0 - sl.cutoff));
        return pointLightComponent * fadeFactor;
    }
    else
    {
        return vec4(0.0, 0.0, 0.0, 0.0);
    }
}

float calculate_directional_light_shadow(vec4 cameraSpacePosition)
{
    vec3 projectedCoords = cameraSpacePosition.xyz / cameraSpacePosition.w;

    vec2 uvCoords;
    uvCoords.x = 0.5 * projectedCoords.x + 0.5;
    uvCoords.y = 0.5 * projectedCoords.y + 0.5;
    float z = (0.5 * projectedCoords.z + 0.5) - 0.05f; // self shadow bias
    
    if (!in_range(uvCoords.x) || !in_range(uvCoords.y) || !in_range(z))
    {
        return 1.0;
    }
    
    return shadow_map_pcf(s_directionalShadowMap, uvCoords, z - u_shadowMapBias, u_shadowMapTexelSize);
}

vec4 calculate_directional_light(DirectionalLight dl, vec4 cameraSpacePosition)
{
    float light = max(dot(v_normal, dl.direction), 0);
    float shadowFactor = calculate_directional_light_shadow(cameraSpacePosition);
    return vec4(dl.color * light * shadowFactor, 1.0);
}

void main()
{
    vec4 ambient = vec4(0.33, 0.25, 0.15, 1.0);
    vec4 totalLighting = vec4(0.0, 0.0, 0.0, 1.0);

    vec3 eyeDir = normalize(u_eye - v_world_position);

    totalLighting += calculate_directional_light(u_directionalLight, v_camera_directional_light);

    for (int i = 0; i < MAX_SPOT_LIGHTS; i++)
        totalLighting += calculate_spot_light(u_spotLights[i], v_camera_spot_light[i], i);

    for (int i = 0; i < MAX_POINT_LIGHTS; i++)
        totalLighting += calculate_point_light(u_pointLights[i].position, u_pointLights[i].color, u_pointLights[i].constantAtten, u_pointLights[i].linearAtten, u_pointLights[i].quadraticAtten, vec4(0, 0, 0, 0), i, POINT_LIGHT);

    f_color = clamp(totalLighting + ambient, 0.0, 1.0);
}