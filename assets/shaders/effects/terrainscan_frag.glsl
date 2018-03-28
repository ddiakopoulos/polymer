#version 330

uniform float u_time;
uniform vec3 u_eye;

uniform float u_scanDistance;
uniform float u_scanWidth;
uniform float u_leadSharp;

uniform vec4 u_leadColor;
uniform vec4 u_midColor;
uniform vec4 u_trailColor;
uniform vec4 u_hbarColor;

uniform float u_nearClip = 2.0;
uniform float u_farClip = 64.0;

uniform sampler2D s_colorTex;
uniform sampler2D s_depthTex;

uniform mat4 u_inverseViewProjection;

uniform vec3 u_scannerPosition = vec3(0, 0, 0);

in vec2 v_texcoord;
in vec3 v_ray;
in vec3 v_world_position;

out vec4 f_color;

#define NUM_OCTAVES 3

float hash(float n) { return fract(sin(n) * 1e4); }
float hash(vec2 p) { return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }

float noise(float x) 
{
    float i = floor(x);
    float f = fract(x);
    float u = f * f * (3.0 - 2.0 * f);
    return mix(hash(i), hash(i + 1.0), u);
}

float noise(vec2 x) 
{
    vec2 i = floor(x);
    vec2 f = fract(x);

    // Four corners in 2D of a tile
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    // Simple 2D lerp using smoothstep envelope between the values.
    // return vec3(mix(mix(a, b, smoothstep(0.0, 1.0, f.x)),
    //          mix(c, d, smoothstep(0.0, 1.0, f.x)),
    //          smoothstep(0.0, 1.0, f.y)));

    // Same code, with the clamps in smoothstep and common subexpressions
    // optimized away.
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float noise(vec3 x) 
{
    const vec3 step = vec3(110, 241, 171);

    vec3 i = floor(x);
    vec3 f = fract(x);
 
    // For performance, compute the base input to a 1D hash from the integer part of the argument and the 
    // incremental change to the 1D based on the 3D -> 1D wrapping
    float n = dot(i, step);

    vec3 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix( hash(n + dot(step, vec3(0, 0, 0))), hash(n + dot(step, vec3(1, 0, 0))), u.x),
                   mix( hash(n + dot(step, vec3(0, 1, 0))), hash(n + dot(step, vec3(1, 1, 0))), u.x), u.y),
               mix(mix( hash(n + dot(step, vec3(0, 0, 1))), hash(n + dot(step, vec3(1, 0, 1))), u.x),
                   mix( hash(n + dot(step, vec3(0, 1, 1))), hash(n + dot(step, vec3(1, 1, 1))), u.x), u.y), u.z);
}

// From Unity CgInc: 
// Used to linearize Z buffer values. x is (1-far/near), y is (far/near), z is (x/far) and w is (y/far).
float linear_01_depth(in float z) 
{
    vec4 zBufferParams = vec4(1.0 - u_farClip/u_nearClip, u_farClip/u_nearClip, 0, 0);
    return (1.00000 / ((zBufferParams.x * z) + zBufferParams.y ));
}

//usage: vec3 reconstructedPos = reconstruct_worldspace_position(v_texcoord, rawDepth);
vec3 reconstruct_worldspace_position(in vec2 coord, in float rawDepth)
{
    vec4 vec = vec4(coord.x, coord.y, rawDepth, 1.0);
    vec = vec * 2.0 - 1.0;                       // (0, 1) to screen coordinates (-1 to 1)
    vec4 r = u_inverseViewProjection * vec;      // deproject into world space
    return r.xyz / r.w;                          // homogenize coordinate
}

vec4 screenspace_bars(vec2 p)
{
    float v = 1.0 - clamp(round(abs(fract(p.y * 100.0) * 2.0)), 0.0, 1.0);
    return vec4(v);
}

void main()
{
    vec4 sceneColor = texture(s_colorTex, v_texcoord);

    float rawDepth = texture(s_depthTex, v_texcoord).x;
    float linearDepth = linear_01_depth(rawDepth);

    // Scale the view ray by the ratio of the linear z value to the projected view ray
    vec3 worldspacePosition = u_eye + (v_ray * linearDepth);

    float dist = distance(worldspacePosition, u_scannerPosition);

    vec4 scannerColor = vec4(0, 0, 0, 0);

    //if (dist < u_scanDistance && dist > u_scanDistance - u_scanWidth)
    {
        float n = noise(worldspacePosition) * 0.5;
        float diff = 1 - (u_scanDistance - dist) / (u_scanWidth);
        vec4 edge = clamp((u_leadColor) * mix(u_midColor, u_leadColor, pow(diff, u_leadSharp)), 0, 1);
        scannerColor = mix(u_trailColor, edge, diff + n) + (screenspace_bars(v_texcoord) * u_hbarColor);
        scannerColor *= diff;
    }

    vec4 outColor = clamp(scannerColor, 0, 1) + sceneColor;
    f_color = vec4(outColor.xyz, 1);
}
