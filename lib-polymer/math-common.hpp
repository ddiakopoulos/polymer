/*
 * File: math-common.hpp
 * This file brings the linalg::aliases namespace into the codebase. It is used
 * as a catch-all: we define useful mathematical constants and introduce a small
 * number of vector functions mirroring their GLSL counterparts. Furthermore, some
 * some common 1D functions are defined (like damped springs and sigmoids). 
 */

#pragma once

#ifndef math_common_hpp
#define math_common_hpp

#include <vector>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <ostream>

#include "linalg.h"

#define POLYMER_PI            3.1415926535897931
#define POLYMER_HALF_PI       1.5707963267948966
#define POLYMER_QUARTER_PI    0.7853981633974483
#define POLYMER_TWO_PI        6.2831853071795862
#define POLYMER_TAU           POLYMER_TWO_PI
#define POLYMER_INV_PI        0.3183098861837907
#define POLYMER_INV_TWO_PI    0.1591549430918953
#define POLYMER_INV_HALF_PI   0.6366197723675813

#define POLYMER_DEG_TO_RAD    0.0174532925199433
#define POLYMER_RAD_TO_DEG    57.295779513082321

#define POLYMER_SQRT_2        1.4142135623730951
#define POLYMER_INV_SQRT_2    0.7071067811865475
#define POLYMER_LN_2          0.6931471805599453
#define POLYMER_INV_LN_2      1.4426950408889634
#define POLYMER_LN_10         2.3025850929940459
#define POLYMER_INV_LN_10     0.43429448190325176

#define POLYMER_GOLDEN        1.61803398874989484820

namespace polymer
{
    using namespace linalg::aliases;

    static const float4x4 Identity4x4 = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    static const float3x3 Identity3x3 = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    static const float2x2 Identity2x2 = {{1, 0}, {0, 1}};
    
    static const float4x4 Zero4x4 = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
    static const float3x3 Zero3x3 = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    static const float2x2 Zero2x2 = {{0, 0}, {0, 0}};

    template<class T> std::ostream & operator << (std::ostream & a, const linalg::vec<T, 2> & b) { return a << '{' << b.x << ", " << b.y << '}'; }
    template<class T> std::ostream & operator << (std::ostream & a, const linalg::vec<T, 3> & b) { return a << '{' << b.x << ", " << b.y << ", " << b.z << '}'; }
    template<class T> std::ostream & operator << (std::ostream & a, const linalg::vec<T, 4> & b) { return a << '{' << b.x << ", " << b.y << ", " << b.z << ", " << b.w << '}'; }

    template<class T, int N> std::ostream & operator << (std::ostream & a, const linalg::mat<T, 2, N> & b) { return a << '\n' << b.row(0) << '\n' << b.row(1) << '\n'; }
    template<class T, int N> std::ostream & operator << (std::ostream & a, const linalg::mat<T, 3, N> & b) { return a << '\n' << b.row(0) << '\n' << b.row(1) << '\n' << b.row(2) << '\n'; }
    template<class T, int N> std::ostream & operator << (std::ostream & a, const linalg::mat<T, 4, N> & b) { return a << '\n' << b.row(0) << '\n' << b.row(1) << '\n' << b.row(2) << '\n' << b.row(3) << '\n'; }

    inline float to_radians(const float degrees) { return degrees * float(POLYMER_PI) / 180.0f; }
    inline float to_degrees(const float radians) { return radians * 180.0f / float(POLYMER_PI); }
    inline double to_radians(const double degrees) { return degrees * POLYMER_PI / 180.0; }
    inline double to_degrees(const double radians) { return radians * 180.0 / POLYMER_PI; }

    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type> T min(const T & x, const T & y) { return ((x) < (y) ? (x) : (y)); }
    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type> T max(const T & x, const T & y) { return ((x) > (y) ? (x) : (y)); }
    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type> T min(const T & a, const T & b, const T & c) { return std::min(a, std::min(b, c)); }
    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type> T max(const T & a, const T & b, const T & c) { return std::max(a, std::max(b, c)); }
    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type> T max(const T & a, const T & b, const T & c, const T & d) { return std::max(a, std::max(b, std::max(c, d))); }
    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type> T clamp(const T & val, const T & min, const T & max) { return std::min(std::max(val, min), max); }
    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type> T normalize(T value, T min, T max) { return clamp<T>((value - min) / (max - min), T(0), T(1)); }
    template<typename T> bool in_range(T val, T min, T max) { return (val >= min && val <= max); }
    template<typename T> T sign(const T & a, const T & b) { return ((b) >= 0.0 ? std::abs(a) : -std::abs(a)); }
    template<typename T> T sign(const T & a) { return (a == 0) ? T(0) : ((a > 0.0) ? T(1) : T(-1)); }
    template<typename T> T rcp(T x) { return T(1) / x; }

    template<class T, int M> linalg::vec<T, M> safe_normalize(const linalg::vec<T, M> & a)
    {
        return a / std::max(T(1E-6), length(a));
    }

    inline float3 project_on_plane(const float3 & I, const float3 & N)
    {
        return I - N * dot(N, I);
    }

    inline float3 reflect(const float3 & I, const float3 & N)
    {
        return I - (N * dot(N, I) * 2.f);
    }

    inline float3 refract(const float3 & I, const float3 & N, float eta)
    {
        float k = 1.0f - eta * eta * (1.0f - dot(N, I) * dot(N, I));
        if (k < 0.0f) return float3();
        else return eta * I - (eta * dot(N, I) + std::sqrt(k)) * N;
    }

    inline float3 faceforward(const float3 & N, const float3 & I, const float3 & Nref)
    {
        return dot(Nref, I) < 0.f ? N : -N;
    }

    // Linear interpolation (mix terminology from GLSL)
    inline float mix(const float a, const float b, const float t)
    {
        return a * (1.0f - t) + b * t;
    }

    // Bilinear interpolation F(u, v) = e + v(f - e)
    inline float interpolate_bilinear(const float a, const float b, const float c, const float d, float u, float v)
    {
        return a * ((1.0f - u) * (1.0f - v)) + b * (u * (1.0f - v)) + c * (v * (1.0f - u)) + d * (u * v);
    }

    template <typename T>
    inline T remap(T value, T inputMin, T inputMax, T outputMin, T outputMax, bool clamp = true)
    {
        T outVal = ((value - inputMin) / (inputMax - inputMin) * (outputMax - outputMin) + outputMin);
        if (clamp)
        {
            if (outputMax < outputMin)
            {
                if (outVal < outputMax) outVal = outputMax;
                else if (outVal > outputMin) outVal = outputMin;
            }
            else
            {
                if (outVal > outputMax) outVal = outputMax;
                else if (outVal < outputMin) outVal = outputMin;
            }
        }
        return outVal;
    }

    inline float damped_spring(const float target, const float current, float & velocity, const float delta, const float spring_constant)
    {
        float currentToTarget = target - current;
        float springForce = currentToTarget * spring_constant;
        float dampingForce = -velocity * 2 * sqrt(spring_constant);
        float force = springForce + dampingForce;
        velocity += force * delta;
        float displacement = velocity * delta;
        return current + displacement;
    }

    // Roughly based on https://graemepottsfolio.wordpress.com/tag/damped-spring/
    inline void critically_damped_spring(const float delta, const float to, const float smooth, const float max_rate, float & x, float & dx)
    {
        if (smooth > 0.f)
        {
            const float omega = 2.f / smooth;
            const float od = omega * delta;
            const float inv_exp = 1.f / (1.f + od + 0.48f * od * od + 0.235f * od * od * od);
            const float change_limit = max_rate * smooth;
            const float clamped = clamp((x - to), -change_limit, change_limit);
            const float t = ((dx + clamped * omega) * delta);
            dx = (dx - t * omega) * inv_exp;
            x = (x - clamped) + ((clamped + t) * inv_exp);
        }
        else if (delta > 0.f)
        {
            const float r = ((to - x) / delta);
            dx = clamp(r, -max_rate, max_rate);
            x += dx * delta;
        }
        else
        {
            x = to;
            dx -= dx;
        }
    }

    inline float smoothstep(const float edge0, const float edge1, const float x)
    {
        const float S = std::min(std::max((x - edge0) / (edge1 - edge0), 0.f), 1.f); // clamp
        return S * S * (3.f - 2.f * S);
    }

    inline float2 smoothstep(const float edge0, const float edge1, const float2 & x)
    {
        return{ smoothstep(edge0, edge1, x.x), smoothstep(edge0, edge1, x.y) };
    }

    inline float3 smoothstep(const float edge0, const float edge1, const float3 & x)
    {
        return{ smoothstep(edge0, edge1, x.x), smoothstep(edge0, edge1, x.y), smoothstep(edge0, edge1, x.z) };
    }

    inline float4 smoothstep(const float edge0, const float edge1, const float4 & x)
    {
        return{ smoothstep(edge0, edge1, x.x), smoothstep(edge0, edge1, x.y), smoothstep(edge0, edge1, x.z), smoothstep(edge0, edge1, x.w) };
    }

    // http://dinodini.wordpress.com/2010/04/05/normalized-tunable-sigmoid-functions/
    inline float sigmoid(float x)
    {
        return 1.f / (1.f + exp(-x));
    }

    inline float normalized_sigmoid(float x, float k)
    {
        float y = 0.f;
        if (x > 0.5f)
        {
            x -= 0.5f;
            k = -1.f - k;
            y = 0.5f;
        }
        return y + (2.f * x * k) / (2.f * (1.f + k - 2.f * x));
    }
}

#endif // end math_common_hpp