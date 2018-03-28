#version 330

#define PI 3.14159265

//

uniform sampler2D u_depthTexture;
uniform sampler2D u_colorTexture;

uniform int u_useNoise;
uniform int u_useMist;
uniform int u_aoOnly;

uniform float u_cameraNearClip;
uniform float u_cameraFarClip;

uniform vec2 u_resolution;

//

in vec2 texCoord;
out vec4 out_color;

//

float width = u_resolution.x;
float height = u_resolution.y;

int samples = 16;

float radius = 3.0;
float edgeHaloClamp = 0.25;

float ditherAmount = 0.0002;

float selfShadowReduction = 0.4;
float gaussianBellWidth = 0.4;

float mistStart = 0.0;
float mistEnd = 16.0;

float lumInfluence = 0.7;

vec2 dithering_noise(vec2 coord)
{
    float noiseX = ((fract(1.0-coord.s*(width/2.0))*0.25) + (fract(coord.t * (height/2.0))*0.75))*2.0-1.0;
    float noiseY = ((fract(1.0-coord.s*(width/2.0))*0.75) + (fract(coord.t * (height/2.0))*0.25))*2.0-1.0;
    if (u_useNoise == 1)
    {
        noiseX = clamp(fract(sin(dot(coord ,vec2(12.9898,78.233))) * 43758.5453),0.0,1.0)*2.0-1.0;
        noiseY = clamp(fract(sin(dot(coord ,vec2(12.9898,78.233)*2.0)) * 43758.5453),0.0,1.0)*2.0-1.0;
    }
    return vec2(noiseX, noiseY) * ditherAmount;
}

float calculate_mist()
{
    float zDepth = texture(u_depthTexture, texCoord.xy).x;
    float mistDepth = -u_cameraFarClip * u_cameraNearClip / (zDepth * (u_cameraFarClip - u_cameraNearClip) - u_cameraFarClip);
    return clamp((mistDepth - mistStart) / mistEnd, 0.0, 1.0);
}

float read_depth(in vec2 coord)
{
    if (texCoord.x < 0.0 || texCoord.y < 0.0)
        return 1.0;
    
    return (2.0 * u_cameraNearClip) / (u_cameraFarClip + u_cameraNearClip - texture(u_depthTexture, coord ).x * (u_cameraFarClip-u_cameraNearClip));
}

float compare_depth(in float depth1, in float depth2, inout int far)
{
    float bellWidth = 2.0;
    float diff = (depth1 - depth2) * 100.0;
    
    // Reduce left bell width to avoid self-shadowing
    if (diff < gaussianBellWidth)
    {
        bellWidth = selfShadowReduction;
    }
    else
    {
        far = 1;
    }
    
    float gauss = pow(2.7182, -2.0 * (diff - gaussianBellWidth) * (diff - gaussianBellWidth) / (bellWidth * bellWidth));
    return gauss;
}

float calculate_ambient_occlusion(float depth, float dw, float dh)
{
    float dd = (1.0-depth) * radius;
    
    float temp = 0.0;
    float temp2 = 0.0;
    float coordw = texCoord.x + dw * dd;
    float coordh = texCoord.y + dh * dd;
    float coordw2 = texCoord.x - dw * dd;
    float coordh2 = texCoord.y - dh * dd;
    
    vec2 coord = vec2(coordw, coordh);
    vec2 coord2 = vec2(coordw2, coordh2);
    
    int far = 0;
    temp = compare_depth(depth, read_depth(coord), far);
    
    if (far > 0)
    {
        temp2 = compare_depth(read_depth(coord2), depth, far);
        temp += (1.0-temp)*temp2;
    }
    
    return temp;
}

void main(void)
{
    vec2 dither = dithering_noise(texCoord);
    float depthSample = read_depth(texCoord);
    
    float w = (1.0 / width) / clamp(depthSample, edgeHaloClamp, 1.0) + (dither.x * (1.0 - dither.x));
    float h = (1.0 / height) / clamp(depthSample, edgeHaloClamp, 1.0) + (dither.y * (1.0 - dither.y));
    
    float pw;
    float ph;
    
    float ao = 0;
    
    float dl = PI * (3.0 - sqrt(5.0));
    float dz = 1.0 / float(samples);
    float l = 0.0;
    float z = 1.0 - dz / 2.0;
    
    for (int i = 0; i <= samples; i++)
    {
        float r = sqrt(1.0 - z);
        
        pw = cos(l) * r;
        ph = sin(l) * r;
        ao += calculate_ambient_occlusion(depthSample, pw * w, ph * h);
        z = z - dz;
        l = l + dl;
    }
    
    ao /= float(samples);
    ao = 1.0 - ao;
    
    if (u_useMist == 1)
    {
        ao = mix(ao, 1.0, calculate_mist());
    }
    
    vec3 color = texture(u_colorTexture, texCoord).rgb;
    
    vec3 lumcoeff = vec3(0.299,0.587,0.114);
    float lum = dot(color.rgb, lumcoeff);
    vec3 luminance = vec3(lum, lum, lum);
    
    vec3 final = vec3(color * mix(vec3(ao), vec3(1.0), luminance * lumInfluence));
    
    if (u_aoOnly == 1)
    {
        final = vec3(mix(vec3(ao), vec3(1.0), luminance * lumInfluence));

    }
    
    out_color = vec4(final, 1.0);
}
