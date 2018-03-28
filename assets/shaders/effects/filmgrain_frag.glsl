#version 330

uniform sampler2D u_diffuseTexture;
uniform float u_Time;
uniform int u_useColoredNoise;
uniform vec2 u_resolution;

const float permTexUnit = 1.0 / 256.0; // Perm texture texel-size
const float permTexUnitHalf = 0.5 / 256.0; // Half perm texture texel-size
const float grainamount = 0.025;

float width = u_resolution.x;
float height = u_resolution.y;

float colorAmount = 0.6;
float grainSize = 1.75; // Approx. Range: (1.5 - 2.5)
float luminanceAmount = 0.66;

in vec2 texCoord;
out vec4 out_color;

// Random texture generator (but you can also use a pre-computed perturbation texture)
vec4 randomTex(in vec2 tc) 
{
    float noise =  sin(dot(tc + vec2(u_Time,u_Time),vec2(12.9898,78.233))) * 43758.5453;

    float noiseR =  fract(noise) * 2.0 - 1.0;
    float noiseG =  fract(noise * 1.2154) * 2.0 - 1.0; 
    float noiseB =  fract(noise * 1.3453) * 2.0 - 1.0;
    float noiseA =  fract(noise * 1.3647) * 2.0 - 1.0;
    
    return vec4(noiseR, noiseG, noiseB, noiseA);
}

float fade(in float t) 
{
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// http://machinesdontcare.wordpress.com/2009/06/25/3d-perlin-noise-sphere-vertex-shader-sourcecode/
float perlinNoise3D(in vec3 p)
{
    vec3 pi = permTexUnit*floor(p)+permTexUnitHalf; // Integer part, scaled so +1 moves permTexUnit texel

    // and offset 1/2 texel to sample texel centers
    vec3 pf = fract(p); // Fractional part for interpolation

    // Noise contributions from (x=0, y=0), z=0 and z=1
    float perm00 = randomTex(pi.xy).a ;
    vec3  grad000 = randomTex(vec2(perm00, pi.z)).rgb * 4.0 - 1.0;
    float n000 = dot(grad000, pf);
    vec3  grad001 = randomTex(vec2(perm00, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
    float n001 = dot(grad001, pf - vec3(0.0, 0.0, 1.0));

    // Noise contributions from (x=0, y=1), z=0 and z=1
    float perm01 = randomTex(pi.xy + vec2(0.0, permTexUnit)).a ;
    vec3  grad010 = randomTex(vec2(perm01, pi.z)).rgb * 4.0 - 1.0;
    float n010 = dot(grad010, pf - vec3(0.0, 1.0, 0.0));
    vec3  grad011 = randomTex(vec2(perm01, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
    float n011 = dot(grad011, pf - vec3(0.0, 1.0, 1.0));

    // Noise contributions from (x=1, y=0), z=0 and z=1
    float perm10 = randomTex(pi.xy + vec2(permTexUnit, 0.0)).a ;
    vec3  grad100 = randomTex(vec2(perm10, pi.z)).rgb * 4.0 - 1.0;
    float n100 = dot(grad100, pf - vec3(1.0, 0.0, 0.0));
    vec3  grad101 = randomTex(vec2(perm10, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
    float n101 = dot(grad101, pf - vec3(1.0, 0.0, 1.0));

    // Noise contributions from (x=1, y=1), z=0 and z=1
    float perm11 = randomTex(pi.xy + vec2(permTexUnit, permTexUnit)).a ;
    vec3  grad110 = randomTex(vec2(perm11, pi.z)).rgb * 4.0 - 1.0;
    float n110 = dot(grad110, pf - vec3(1.0, 1.0, 0.0));
    vec3  grad111 = randomTex(vec2(perm11, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
    float n111 = dot(grad111, pf - vec3(1.0, 1.0, 1.0));

    // Blend contributions along x
    vec4 n_x = mix(vec4(n000, n001, n010, n011), vec4(n100, n101, n110, n111), fade(pf.x));

    // Blend contributions along y
    vec2 n_xy = mix(n_x.xy, n_x.zw, fade(pf.y));

    // Blend contributions along z
    float n_xyz = mix(n_xy.x, n_xy.y, fade(pf.z));
    
    return n_xyz;
}

// 2D texture coordinate rotation
vec2 rotateCoordinate2D(in vec2 tc, in float angle)
{
    float aspect = width / height;
    float rotX = ((tc.x * 2.0 - 1.0) * aspect * cos(angle)) - ((tc.y * 2.0 - 1.0) * sin(angle));
    float rotY = ((tc.y * 2.0 - 1.0) * cos(angle)) + ((tc.x * 2.0 - 1.0) * aspect * sin(angle));
    rotX = ((rotX / aspect) * 0.5 + 0.5);
    rotY = rotY * 0.5 + 0.5;
    return vec2(rotX,rotY);
}

void main() 
{
    vec3 rotOffset = vec3(1.425,3.892,5.835); //rotation offset values  
    vec2 rotCoordsR = rotateCoordinate2D(texCoord, u_Time + rotOffset.x);
    vec3 noise = vec3(perlinNoise3D(vec3(rotCoordsR * vec2(width / grainSize, height / grainSize), 0.0)));
  
    if (u_useColoredNoise == 1)
    {
        vec2 rotCoordsG = rotateCoordinate2D(texCoord, u_Time + rotOffset.y);
        vec2 rotCoordsB = rotateCoordinate2D(texCoord, u_Time + rotOffset.z);
        noise.g = mix(noise.r, perlinNoise3D(vec3(rotCoordsG * vec2(width / grainSize, height / grainSize), 1.0)), colorAmount);
        noise.b = mix(noise.r, perlinNoise3D(vec3(rotCoordsB * vec2(width / grainSize, height / grainSize), 2.0)), colorAmount);
    }

    vec3 final = texture(u_diffuseTexture, texCoord).rgb;

    // Noisiness response curve based on scene luminance
    vec3 lumcoeff = vec3(0.299,0.587,0.114);
    float luminance = mix(0.0,dot(final, lumcoeff),luminanceAmount);
    float lum = smoothstep(0.2,0.0,luminance);

    lum += luminance;
    
    noise = mix(noise,vec3(0.0),pow(lum,4.0));
    final = final + noise * grainamount;
   
    out_color = vec4(final, 1.0);
}
