/*
 * File: math-color.hpp
 */

#pragma once 

#ifndef math_color_hpp
#define math_color_hpp

#include "math-common.hpp"

namespace polymer
{
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
