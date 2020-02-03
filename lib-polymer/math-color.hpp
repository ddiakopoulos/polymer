/*
 * File: math-color.hpp
 */

#pragma once 

#ifndef math_color_hpp
#define math_color_hpp

#include "math-common.hpp"

namespace polymer
{
    #define POLYMER_GAMMA 2.2f

    inline float4 premultiply_alpha(const float4 & color) { return { color.xyz() * color.w, color.w }; }
    inline float4 unpremultiply_alpha(const float4 & color) { return { color.xyz() / color.w, color.w }; }

    inline float srgb_to_linear(const float c) { return (c <= 0.04045f) ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, POLYMER_GAMMA); }
    inline float linear_to_srgb(const float c) { return (c <= 0.0031308f) ? c * 12.92f : 1.055f * std::pow(c, 1.0f / POLYMER_GAMMA) - 0.055f; }
    inline float3 srgb_to_linear(const float3 & c) { return { srgb_to_linear(c.x), srgb_to_linear(c.y), srgb_to_linear(c.z) }; }
    inline float3 linear_to_srgb(const float3 & c) { return { linear_to_srgb(c.x), linear_to_srgb(c.y), linear_to_srgb(c.z) }; }
    inline float4 srgb_to_linear(const float4 & c) { return { srgb_to_linear(c.x), srgb_to_linear(c.y), srgb_to_linear(c.z), c.w}; }
    inline float4 linear_to_srgb(const float4 & c) { return { linear_to_srgb(c.x), linear_to_srgb(c.y), linear_to_srgb(c.z), c.w}; }

    // https://en.wikipedia.org/wiki/Luminance
    // Only valid for linear RGB space (Rec. 709)
    inline float luminance(const float4 & linear_rgb_color)
    {
        return 0.2126f * linear_rgb_color.x + 0.7152f * linear_rgb_color.y + 0.0722f * linear_rgb_color.z;
    }

    // https://en.wikipedia.org/wiki/YCoCg
    inline float4 rgba_to_ycocg(const float4 & c)
    {
        return { 0.25f * (c.x + 2.0f * c.y + c.z), c.x - c.z, c.y - 0.5f * (c.x + c.z), c.w };
    }

    inline float4 ycocg_to_rgba(const float4 & c)
    {
        return { c.x + 0.5f * (c.y - c.z), c.x + 0.5f * c.z, c.x - 0.5f * (c.y + c.z), c.w };
    }

    inline float4 rgb_to_xyz(const float4 & c)
    {
        float x, y, z, r, g, b;

        r = c.x / 255.0f; g = c.y / 255.0f; b = c.z / 255.0f;

        if (r > 0.04045f) r = std::powf(((r + 0.055f) / 1.055f), 2.4f);
        else r /= 12.92f;

        if (g > 0.04045f) g = std::powf(((g + 0.055f) / 1.055f), 2.4f);
        else g /= 12.92f;

        if (b > 0.04045f) b = std::powf(((b + 0.055f) / 1.055f), 2.4f);
        else b /= 12.92f;

        r *= 100.f; g *= 100.f; b *= 100.f;

        x = r * 0.4124f + g * 0.3576f + b * 0.1805f;
        y = r * 0.2126f + g * 0.7152f + b * 0.0722f;
        z = r * 0.0193f + g * 0.1192f + b * 0.9505f;

        return { x, y, z, c.w };
    }

    inline float4 xyz_to_cielab(const float4 & c)
    {
        float x, y, z, l, a, b;
        const float refX = 95.047f, refY = 100.0f, refZ = 108.883f;

        x = c.x / refX; y = c.y / refY; z = c.z / refZ;

        if (x > 0.008856f) x = std::pow(x, 1.f / 3.0f);
        else x = (7.787f * x) + (16.0f / 116.0f);

        if (y > 0.008856f) y = std::pow(y, 1.f / 3.0f);
        else y = (7.787f * y) + (16.0f / 116.0f);

        if (z > 0.008856f) z = std::pow(z, 1.f / 3.0f);
        else z = (7.787f * z) + (16.0f / 116.0f);

        l = 116.f * y - 16.f;
        a = 500.f * (x - y);
        b = 200.f * (y - z);

        return { l, a, b, c.w };
    }

    // Compute perceptual ΔE using CIE1976
    // https://en.wikipedia.org/wiki/Color_difference
    // RGB->XYZ->CIELAB conversion is performed using D65 illuminant
    inline float compute_delta_e(const float4 & a, const float4 & b)
    {
        const auto lab_a = xyz_to_cielab(rgb_to_xyz(a));
        const auto lab_b = xyz_to_cielab(rgb_to_xyz(b));
        return std::sqrtf(std::powf(lab_a.x - lab_b.x, 2) + std::powf(lab_a.y - lab_b.y, 2) + std::powf(lab_a.z - lab_b.z, 2));
    }

    inline float3 rgb_to_hsv(const float3 & rgb)
    {
        float rd = rgb.x / 255;
        float gd = rgb.y / 255;
        float bd = rgb.z / 255;

        float max = polymer::max(rd, gd, bd), min = polymer::min(rd, gd, bd);
        float h, s, v = max;

        float d = max - min;
        s = max == 0 ? 0 : d / max;

        if (max == min)
        {
            h = 0; // achromatic
        }
        else
        {
            if (max == rd) h = (gd - bd) / d + (gd < bd ? 6 : 0);
            else if (max == gd) h = (bd - rd) / d + 2;
            else if (max == bd) h = (rd - gd) / d + 4;
            h /= 6;
        }

        return float3(h, s, v);
    }

    inline float3 hsv_to_rgb(const float3 & hsv)
    {
        float r, g, b;

        float h = hsv.x;
        float s = hsv.y;
        float v = hsv.z;

        int i = int(h * 6);

        float f = h * 6 - i;
        float p = v * (1 - s);
        float q = v * (1 - f * s);
        float t = v * (1 - (1 - f) * s);

        switch (i % 6)
        {
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        case 5: r = v, g = p, b = q; break;
        }

        float3 rgb;
        rgb.x = uint8_t(clamp((float)r * 255.f, 0.f, 255.f));
        rgb.y = uint8_t(clamp((float)g * 255.f, 0.f, 255.f));
        rgb.z = uint8_t(clamp((float)b * 255.f, 0.f, 255.f));
        return rgb;
    }

    inline float3 interpolate_color_hsv(const float3 & rgb_a, const float3 & rgb_b, const float t)
    {
        auto aHSV = rgb_to_hsv(rgb_a);
        auto bHSV = rgb_to_hsv(rgb_b);
        auto result = linalg::lerp(aHSV, bHSV, t);
        return hsv_to_rgb(float3(result));
    }

    inline float3 hsl_to_rgb(const float3 & hsl)
    {
        auto hue_to_rgb = [&](float m1, float m2, float h)
        {
            while (h < 0) h += 1;
            while (h > 1) h -= 1;
            if (h * 6.0f < 1) return m1 + (m2 - m1) * h * 6;
            if (h * 2.0f < 1) return m2;
            if (h * 3.0f < 2) return m1 + (m2 - m1) * (2.0f / 3.0f - h) * 6;
            return m1;
        };

        float h = hsl.x;
        float s = hsl.y;
        float l = hsl.z;

        if (l < 0) l = 0;
        if (s < 0) s = 0;
        if (l > 1) l = 1;
        if (s > 1) s = 1;
        while (h < 0) h += 1;
        while (h > 1) h -= 1;

        if (s == 0) s = 1e-10f;

        float m2;
        if (l <= 0.5f) m2 = l * (s + 1.0f);
        else m2 = (l + s) - (l * s);
        float m1 = (l * 2.0f) - m2;

        const float r = hue_to_rgb(m1, m2, h + 1.0f / 3.0f);
        const float g = hue_to_rgb(m1, m2, h);
        const float b = hue_to_rgb(m1, m2, h - 1.0f / 3.0f);

        return float3(r, g, b);
    }

    inline float3 rgb_to_hsl(const float3 & rgb) 
    {
        float r = rgb.r / 255.f;
        float g = rgb.g / 255.f;
        float b = rgb.b / 255.f;

        float max = std::max(r, std::max(g, b));
        float min = std::min(r, std::min(g, b));
        float delta = max - min;

        float h = 0, s = 0, l = (max + min) / 2.0f;

        if (max == min) 
        {
            h = s = 0; // achromatic
        }
        else 
        {
            if (l < 0.5f) s = delta / (max + min);
            else s = delta / (2.0f - max - min);
            if (r == max) h = (g - b) / delta + (g < b ? 6 : 0);
            else if (g == max) h = (b - r) / delta + 2;
            else if (b == max) h = (r - g) / delta + 4;
        }

        return { h / 6 * 360, s * 100, l * 100 };
    }

}

#endif // end math_color_hpp
