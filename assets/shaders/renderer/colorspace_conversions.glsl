#version 450

float linearrgb_to_srgb1(const in float c, const in float gamma) 
{
    float v = 0.0;
    if(c < 0.0031308) 
    {
        if (c > 0.0) v = c * 12.92;
    } 
    else 
    {
        v = 1.055 * pow(c, 1.0/ gamma) - 0.055;
    }
    return v;
}

vec4 linearTosRGB(const in vec4 from, const in float gamma)
{
    vec4 to;
    to.r = linearrgb_to_srgb1(from.r, gamma);
    to.g = linearrgb_to_srgb1(from.g, gamma);
    to.b = linearrgb_to_srgb1(from.b, gamma);
    to.a = from.a;
    return to;
}

vec3 linearTosRGB(const in vec3 from, const in float gamma)
{
    vec3 to;
    to.r = linearrgb_to_srgb1(from.r, gamma);
    to.g = linearrgb_to_srgb1(from.g, gamma);
    to.b = linearrgb_to_srgb1(from.b, gamma);
    return to;
}

float sRGBToLinear(const in float c, const in float gamma)
{
    float v = 0.0;
    if (c < 0.04045) 
    {
        if (c >= 0.0) v = c * (1.0 / 12.92);
    } 
    else 
    {
        v = pow((c + 0.055) * (1.0 / 1.055), gamma);
    }
    return v;
}

vec4 sRGBToLinear(const in vec4 from, const in float gamma)
{
    vec4 to;
    to.r = sRGBToLinear(from.r, gamma);
    to.g = sRGBToLinear(from.g, gamma);
    to.b = sRGBToLinear(from.b, gamma);
    to.a = from.a;
    return to;
}

vec3 sRGBToLinear(const in vec3 from, const in float gamma)
{
    vec3 to;
    to.r = sRGBToLinear(from.r, gamma);
    to.g = sRGBToLinear(from.g, gamma);
    to.b = sRGBToLinear(from.b, gamma);
    return to;
}