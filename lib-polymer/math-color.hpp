/*
 * File: math-color.hpp
 */

#pragma once 

#ifndef math_color_hpp
#define math_color_hpp

#include "math-common.hpp"

namespace polymer
{
    inline float to_luminance(const float4 & color)
    {
        return 0.2126f * color.x + 0.7152f * color.y + 0.0722f;
    }

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

        r = c.x / 255.0; g = c.y / 255.0; b = c.z / 255.0;

        if (r > 0.04045) r = std::powf(((r + 0.055) / 1.055), 2.4);
        else r /= 12.92;

        if (g > 0.04045) g = std::powf(((g + 0.055) / 1.055), 2.4);
        else g /= 12.92;

        if (b > 0.04045) b = std::powf(((b + 0.055) / 1.055), 2.4);
        else b /= 12.92;

        r *= 100.f; g *= 100.f; b *= 100.f;

        x = r * 0.4124 + g * 0.3576 + b * 0.1805;
        y = r * 0.2126 + g * 0.7152 + b * 0.0722;
        z = r * 0.0193 + g * 0.1192 + b * 0.9505;

        return { x, y, z, c.w };
    }

    inline float4 xyz_to_cielab(const float4 & c)
    {
        float x, y, z, l, a, b;
        const float refX = 95.047, refY = 100.0, refZ = 108.883;

        x = c.x / refX; y = c.y / refY; z = c.z / refZ;

        if (x > 0.008856) x = powf(x, 1 / 3.0);
        else x = (7.787 * x) + (16.0 / 116.0);

        if (y > 0.008856) y = powf(y, 1 / 3.0);
        else y = (7.787 * y) + (16.0 / 116.0);

        if (z > 0.008856) z = powf(z, 1 / 3.0);
        else z = (7.787 * z) + (16.0 / 116.0);

        l = 116 * y - 16;
        a = 500 * (x - y);
        b = 200 * (y - z);

        return { l, a, b, c.w };
    }

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
            if (max == rd)  h = (gd - bd) / d + (gd < bd ? 6 : 0);
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
        float3 aHSV = rgb_to_hsv(rgb_a);
        float3 bHSV = rgb_to_hsv(rgb_b);
        float3 result = linalg::lerp(aHSV, bHSV, t);
        return hsv_to_rgb(result);
    }

    struct color_hsl
    {
        float h{0.f};
        float s{0.f};
        float l{0.f};
    };

    inline float3 hsl_to_rgb(const color_hsl & hsl)
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

        float h = hsl.h;
        float s = hsl.s;
        float l = hsl.l;

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

}

#endif // end math_color_hpp
