#version 330

uniform float u_time;
uniform sampler2D s_outerTex;
uniform sampler2D s_innerTex;

in vec3 v_position;
in vec2 v_texcoord;

out vec4 f_color;

// <https://www.shadertoy.com/view/4dS3Wd>

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

float colormap_red(float x) 
{
    return (1.0 + 1.0 / 63.0) * x - 1.0 / 63.0;
}

float colormap_green(float x) 
{
    return -(1.0 + 1.0 / 63.0) * x + (1.0 + 1.0 / 63.0);
}

float pulse(float t) 
{
    float pi = 3.14159;
    float freq = 0.1; 
    return 0.5*(1+sin(2 * pi * freq * t));
}

vec3 colormap(float x) 
{
    float r = clamp(colormap_red(x), 0.0, 1.0);
    float g = clamp(colormap_green(x), 0.0, 1.0);
    float b = 1.0;
    return vec3(r, g, b);
}

void main() 
{
    vec4 tex = texture2D(s_outerTex, v_texcoord);
    float n = noise(u_time);
    float v = pulse(u_time + (gl_FragCoord.y / 150.0));
    f_color = vec4(colormap(v), tex.a);
}