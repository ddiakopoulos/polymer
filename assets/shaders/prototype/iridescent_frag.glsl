#version 330

in vec3 v_position, v_normal;
in vec2 v_texcoord;
in vec3 v_eyeDir;

out vec4 f_color;

uniform float u_iridescentPower = 1.0;
uniform float u_time;

// Noise via https://www.shadertoy.com/view/4dS3Wd

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

float fbm(float x) 
{
    float v = 0.0;
    float a = 0.5;
    float shift = float(100);
    for (int i = 0; i < NUM_OCTAVES; ++i) 
    {
        v += a * noise(x);
        x = x * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

float fbm(vec2 x) 
{
    float v = 0.0;
    float a = 0.5;
    vec2 shift = vec2(100);
    mat2 rot = mat2(cos(0.5), sin(0.5), -sin(0.5), cos(0.50)); // Rotate to reduce axial bias
    for (int i = 0; i < NUM_OCTAVES; ++i) 
    {
        v += a * noise(x);
        x = rot * x * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

float fbm(vec3 x) 
{
    float v = 0.0;
    float a = 0.5;
    vec3 shift = vec3(100);
    for (int i = 0; i < NUM_OCTAVES; ++i) 
    {
        v += a * noise(x);
        x = x * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

float map(float value, float oMin, float oMax, float iMin, float iMax)
{
    return iMin + ((value - oMin) / (oMax - oMin)) * (iMax - iMin);
}

// https://vimeo.com/83798053
vec3 compute_iridescence(float orientation, vec3 position)
{
    float frequency = 4.0;
    float offset = 4.0 * (0.05 * u_time);
    float noiseInc = 10.0;

    vec3 color;
    color.x = abs(cos(orientation * frequency + fbm(position) * noiseInc + 1 + offset));
    color.y = abs(cos(orientation * frequency + fbm(position) * noiseInc + 2 + offset));
    color.z = abs(cos(orientation * frequency + fbm(position) * noiseInc + 3 + offset));

    return color;
}

void main()
{   
    vec3 vert = gl_FrontFacing ? normalize(-v_position) : normalize(v_position);

    float facingRatio = dot(v_normal, v_eyeDir); // or v_normal with vert for a patterned look

    vec4 iridescentColor = vec4(compute_iridescence(facingRatio, v_position), 1.0) * 
                            map(pow(facingRatio, u_iridescentPower), 0.0, 1.0, 0.5, 1); // or 1 - pow

    f_color = vec4(iridescentColor.rgb, 1.0);
}
