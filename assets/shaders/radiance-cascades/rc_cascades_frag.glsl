#version 450

in vec2 v_texcoord;
out vec3 frag_radiance;

uniform vec2 u_resolution;
uniform sampler2D u_scene_texture;
uniform sampler2D u_distance_texture;
uniform sampler2D u_last_texture;
uniform vec2 u_cascade_extent;
uniform float u_cascade_count;
uniform float u_cascade_index;
uniform float u_base_pixels_between_probes;
uniform float u_cascade_interval;
uniform float u_ray_interval;
uniform float u_interval_overlap;
uniform float u_base_ray_count;
uniform float u_srgb;
uniform int u_enable_sun;
uniform float u_sun_angle;
uniform int u_add_noise;
uniform float u_first_cascade_index;
uniform int u_bilinear_fix_enabled;

const float SQRT_2 = 1.41;
const float POLYMER_PI = 3.14159265;
const float POLYMER_TAU = 2.0 * POLYMER_PI;
const float goldenAngle = POLYMER_PI * 0.7639320225;
const float sunDistance = 1.0;

const vec3 skyColor = vec3(0.2, 0.24, 0.35) * 4.0;
const vec3 sunColor = vec3(0.95, 0.9, 0.8) * 3.0;

const vec3 oldSkyColor = vec3(0.02, 0.08, 0.2);
const vec3 oldSunColor = vec3(0.95, 0.95, 0.9);

vec3 oldSunAndSky(float rayAngle)
{
    float angleToSun = mod(rayAngle - u_sun_angle, POLYMER_TAU);
    float sunIntensity = smoothstep(1.0, 0.0, angleToSun);
    return oldSunColor * sunIntensity + oldSkyColor;
}

vec3 sunAndSky(float rayAngle)
{
    float angleToSun = mod(rayAngle - u_sun_angle, POLYMER_TAU);
    float sunIntensity = pow(max(0.0, cos(angleToSun)), 4.0 / sunDistance);
    return mix(sunColor * sunIntensity, skyColor, 0.3);
}

float rand(vec2 co)
{
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec4 colorSample(sampler2D tex, vec2 uv, bool srgbSample)
{
    vec4 color = texture(tex, uv);
    if (!srgbSample) return color;
    return vec4(pow(color.rgb, vec3(u_srgb)), color.a);
}

vec4 raymarch(vec2 rayStart, vec2 rayEnd, float scale, vec2 oneOverSize, float minStepSize)
{
    vec2 rayDir = normalize(rayEnd - rayStart);
    float rayLength = length(rayEnd - rayStart);

    vec2 rayUv = rayStart * oneOverSize;

    for (float dist = 0.0; dist < rayLength;)
    {
        if (any(lessThan(rayUv, vec2(0.0))) || any(greaterThan(rayUv, vec2(1.0))))
            break;

        float df = textureLod(u_distance_texture, rayUv, 0.0).r;

        if (df <= minStepSize)
        {
            vec4 sampleLight = textureLod(u_scene_texture, rayUv, 0.0);
            sampleLight.rgb = pow(sampleLight.rgb, vec3(u_srgb));
            return sampleLight;
        }

        dist += df * scale;
        rayUv += rayDir * (df * scale * oneOverSize);
    }

    return vec4(0.0);
}

vec2 getUpperCascadeTextureUv(float index, vec2 offset, float spacingBase)
{
    float upperSpacing = pow(spacingBase, u_cascade_index + 1.0);
    vec2 upperSize = floor(u_cascade_extent / upperSpacing);
    vec2 upperPosition = vec2(
        mod(index, upperSpacing),
        floor(index / upperSpacing)
    ) * upperSize;

    vec2 clamped = clamp(offset, vec2(0.5), upperSize - 0.5);
    return (upperPosition + clamped) / u_cascade_extent;
}

vec4 merge(vec4 currentRadiance, float index, vec2 position, float spacingBase, vec2 localOffset)
{
    if (currentRadiance.a > 0.0 || u_cascade_index >= max(1.0, u_cascade_count - 1.0))
    {
        return currentRadiance;
    }

    vec2 offset = (position + localOffset) / spacingBase;
    vec2 upperProbePosition = getUpperCascadeTextureUv(index, offset, spacingBase);

    vec3 upperSample = textureLod(
        u_last_texture,
        upperProbePosition,
        u_base_pixels_between_probes == 1.0 ? 0.0 : log(u_base_pixels_between_probes) / log(2.0)
    ).rgb;

    return currentRadiance + vec4(upperSample, 1.0);
}

void main()
{
    vec2 coord = floor(v_texcoord * u_cascade_extent);

    float base = u_base_ray_count;
    float rayCount = pow(base, u_cascade_index + 1.0);
    float spacingBase = sqrt(u_base_ray_count);
    float spacing = pow(spacingBase, u_cascade_index);

    // Hand-wavy rule that improved smoothing of other base ray counts
    float modifierHack = base < 16.0 ? pow(u_base_pixels_between_probes, 1.0) : spacingBase;

    vec2 size = floor(u_cascade_extent / spacing);
    vec2 probeRelativePosition = mod(coord, size);
    vec2 rayPos = floor(coord / size);

    float modifiedInterval = modifierHack * u_ray_interval * u_cascade_interval;

    float start = (u_cascade_index == 0.0 ? 0.0 : pow(base, u_cascade_index - 1.0)) * modifiedInterval;
    float end = ((1.0 + 3.0 * u_interval_overlap) * pow(base, u_cascade_index) - pow(u_cascade_index, 2.0)) * modifiedInterval;

    vec2 interval = vec2(start, end);

    vec2 probeCenter = (probeRelativePosition + 0.5) * u_base_pixels_between_probes * spacing;

    float preAvgAmt = u_base_ray_count;

    float baseIndex = (rayPos.x + spacing * rayPos.y) * preAvgAmt;
    float angleStep = POLYMER_TAU / rayCount;

    float scale = min(u_resolution.x, u_resolution.y);
    vec2 oneOverSize = 1.0 / u_resolution;
    float minStepSize = min(oneOverSize.x, oneOverSize.y) * 0.5;
    float avgRecip = 1.0 / preAvgAmt;

    vec2 normalizedProbeCenter = probeCenter * oneOverSize;

    vec4 totalRadiance = vec4(0.0);
    float noise = (u_add_noise == 1) ? rand(v_texcoord * (u_cascade_index + 1.0)) : 0.0;

    for (int i = 0; i < int(preAvgAmt); i++)
    {
        float index = baseIndex + float(i);
        float angle = (index + 0.5 + noise) * angleStep;
        vec2 rayDir = vec2(cos(angle), -sin(angle));
        vec2 rayStart = probeCenter + rayDir * interval.x;
        vec2 rayEnd = rayStart + rayDir * interval.y;
        vec4 raymarched = raymarch(rayStart, rayEnd, scale, oneOverSize, minStepSize);
        vec4 mergedRadiance = merge(raymarched, index, probeRelativePosition, spacingBase, vec2(0.5));

        if (u_enable_sun == 1 && u_cascade_index == u_cascade_count - 1.0)
        {
            mergedRadiance.rgb = max((u_add_noise == 1) ? oldSunAndSky(angle) : sunAndSky(angle), mergedRadiance.rgb);
        }

        totalRadiance += mergedRadiance * avgRecip;
    }

    frag_radiance = (u_cascade_index > u_first_cascade_index)
        ? totalRadiance.rgb
        : pow(totalRadiance.rgb, vec3(1.0 / u_srgb));
}
