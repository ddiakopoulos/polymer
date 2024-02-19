/*
 * This file incorporates Public Domain and BSD-2 code from:
 * Simon Geilfus, Stefan Gustavson, Robert Bridson, and Iñigo Quilez.
 * See COPYING file for full attribution information.
 */ 

#pragma once

#ifndef polymer_simplex_noise_hpp
#define polymer_simplex_noise_hpp

#include "polymer-core/util/util.hpp"
#include "polymer-core/math/math-common.hpp"

#include <random>

#if defined(POLYMER_PLATFORM_WINDOWS)
#pragma warning(push)
#pragma warning(disable : 4244)
#endif

namespace noise 
{

// This brings back the returned noise of the dnoise functions into -1,1 range. For some reason this is not the case in Stefan Gustavson implementation
//#define SIMPLEX_DERIVATIVES_RESCALE

using namespace polymer;

///////////////////////////////////
//   Dimensional Simplex Noise   //
///////////////////////////////////

float noise(float x);
float noise(const float2 & v);
float noise(const float3 & v);
float noise(const float4 & v);

//////////////////////////////
//   Ridged Simplex Noise   //
//////////////////////////////

float noise_ridged(float x);
float noise_ridged(const float2 & v);
float noise_ridged(const float3 & v);
float noise_ridged(const float4 & v);

/////////////////////////////////////////////////
//   Simplex Noise Via Analytical Derivative   //
/////////////////////////////////////////////////

float2 noise_deriv(float x);
float3 noise_deriv(const float2 & v);
float4 noise_deriv(const float3 & v);
std::array<float,5> noise_deriv(const float4 & v);
    
//////////////////////////////////////////
//   2D Simplex Worley/Cellular Noise   //
//////////////////////////////////////////

float noise_worley(const float2 & v);
float noise_worley(const float3 & v);
float noise_worley(const float2 & v, float falloff);
float noise_worley(const float3 & v, float falloff);

//////////////////////////////////////////////////////////
//   2D/3D Simplex Flow Noise with Rotating Gradients   //
//////////////////////////////////////////////////////////

float noise_flow(const float2 & v, float angle);
float noise_flow(const float3 & v, float angle);

////////////////////////////////////////////////////////////
//   2D/3D Simplex Flow Noise Via Analytical Derivative   //
////////////////////////////////////////////////////////////

float3 noise_flow_deriv(const float2 & v, float angle);
float4 noise_flow_deriv(const float3 & v, float angle);

//////////////////////////////////////////
//   Compute Curl of 2D Simplex Noise   //
//////////////////////////////////////////

float2 noise_curl(const float2 & v); // 2d simplex noise
float2 noise_curl(const float2 & v, float t); // 2d simplex flow noise
float2 noise_curl(const float2 & v, uint8_t octaves, float lacunarity, float gain); // 2d fractal brownian motion sum
float3 noise_curl(const float3 & v); // 3d simplex noise
float3 noise_curl(const float3 & v, float t); // 3d simplex flow noise
float3 noise_curl(const float3 & v, uint8_t octaves, float lacunarity, float gain); // 3D simplex noise fractal brownian motion sum

float2 curl(const float2 & v, const std::function<float(const float2&)> &potential, float delta = 1e-4f); // curl of a custom 2D potential using finite difference approximation
float3 curl(const float3 & v, const std::function<float3(const float3&)> &potential, float delta = 1e-4f); // curl of a custom 3D potential using finite difference approximation

/////////////////////////////////
//   Fractal Brownian Motion   //
/////////////////////////////////

float noise_fb(float x, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);
float noise_fb(const float2 & v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);
float noise_fb(const float3 & v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);
float noise_fb(const float4 & v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);

///////////////////////////////////////////////////////////
//   Fractal Brownian Motion via Analytical Derivative   //
///////////////////////////////////////////////////////////

float2 noise_fb_deriv(float x, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);
float3 noise_fb_deriv(const float2 & v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);
float4 noise_fb_deriv(const float3 & v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);
std::array<float,5> noise_fb_deriv(const float4 & v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);
    
////////////////////////////////////////
//   Ridged Multi-Fractal Noise Sum   //
////////////////////////////////////////

float noise_ridged_mf(float x, float ridgeOffset = 1.0f, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);
float noise_ridged_mf(const float2 & v, float ridgeOffset = 1.0f, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);
float noise_ridged_mf(const float3 & v, float ridgeOffset = 1.0f, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);
float noise_ridged_mf(const float4 & v, float ridgeOffset = 1.0f, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);

/////////////////////////////////////////////////////
//   Noise Fractal Variation Via Iñigo's Methods   //
/////////////////////////////////////////////////////

float noise_iq_fb(const float2 & v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);
float noise_iq_fb(const float3 & v, uint8_t octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);
float noise_iq_fb(const float2 & v, uint8_t octaves = 4, const float2x2 & mat = float2x2({1.6f, -1.2f}, {1.2f, 1.6f}), float gain = 0.5f); // mat2 to transform each octave

///////////////
//   Utils   //
///////////////

// Skewing factors for 2D simplex grid:
// F2 = 0.5*(sqrt(3.0)-1.0)
// G2 = (3.0-Math.sqrt(3.0))/6.0
static const float F2 = 0.366025403f;
static const float G2 = 0.211324865f;

// Skewing factors for 3D simplex grid:
// F3 = 1/3
// G3 = 1/6 
static const float F3 = 0.333333333f;
static const float G3 = 0.166666667f;

// Skewing factors for 4D simplex grid:
// F4 = (Math.sqrt(5.0)-1.0)/4.0
// G4 = (5.0-Math.sqrt(5.0))/20.0
static const float F4 = 0.309016994f; 
static const float G4 = 0.138196601f;

inline float fast_floor(const float & x)
{
    int xi = (int) x; return x < xi ? xi - 1 : xi;
}

namespace impl 
{
    // Permutation table. This is just a random jumble of all numbers 0-255,
    // repeated twice to avoid wrapping the index at 255 for each lookup.
    // This needs to be exactly the same for all instances on all platforms,
    // so it's easiest to just keep it as static explicit data.
    static uint8_t s_perm_table[512] = {
        151,160,137,91,90,15,
        131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
        190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
        88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
        77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
        102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
        135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
        5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
        223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
        129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
        251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
        49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
        138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
        151,160,137,91,90,15,
        131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
        190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
        88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
        77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
        102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
        135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
        5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
        223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
        129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
        251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
        49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
        138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
    };
    
    // Gradient tables. These could be programmed the Ken Perlin way with
    // some clever bit-twiddling, but this is more clear, and not really slower.
    static float s_gradient_2_table[8][2] = {
        { -1.0f, -1.0f }, { 1.0f, 0.0f } , { -1.0f, 0.0f } , { 1.0f, 1.0f } ,
        { -1.0f, 1.0f } , { 0.0f, -1.0f } , { 0.0f, 1.0f } , { 1.0f, -1.0f }
    };
    
    // Gradient directions for 3D.
    // These vectors are based on the midpoints of the 12 edges of a cube.
    // A larger array of random unit length vectors would also do the job,
    // but these 12 (including 4 repeats to make the array length a power
    // of two) work better. They are not random, they are carefully chosen
    // to represent a small, isotropic set of directions.
    
    static float s_gradient_3_table[16][3] = {
        { 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 1.0f }, // 12 cube edges
        { -1.0f, 0.0f, 1.0f }, { 0.0f, -1.0f, 1.0f },
        { 1.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, -1.0f },
        { -1.0f, 0.0f, -1.0f }, { 0.0f, -1.0f, -1.0f },
        { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f },
        { -1.0f, 1.0f, 0.0f }, { -1.0f, -1.0f, 0.0f },
        { 1.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 1.0f }, // 4 repeats to make 16
        { 0.0f, 1.0f, -1.0f }, { 0.0f, -1.0f, -1.0f }
    };
    
    static float s_gradient_4_table[32][4] = {
        { 0.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f, -1.0f, 1.0f }, { 0.0f, 1.0f, -1.0f, -1.0f }, // 32 tesseract edges
        { 0.0f, -1.0f, 1.0f, 1.0f }, { 0.0f, -1.0f, 1.0f, -1.0f }, { 0.0f, -1.0f, -1.0f, 1.0f }, { 0.0f, -1.0f, -1.0f, -1.0f },
        { 1.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, -1.0f }, { 1.0f, 0.0f, -1.0f, 1.0f }, { 1.0f, 0.0f, -1.0f, -1.0f },
        { -1.0f, 0.0f, 1.0f, 1.0f }, { -1.0f, 0.0f, 1.0f, -1.0f }, { -1.0f, 0.0f, -1.0f, 1.0f }, { -1.0f, 0.0f, -1.0f, -1.0f },
        { 1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, -1.0f }, { 1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, -1.0f, 0.0f, -1.0f },
        { -1.0f, 1.0f, 0.0f, 1.0f }, { -1.0f, 1.0f, 0.0f, -1.0f }, { -1.0f, -1.0f, 0.0f, 1.0f }, { -1.0f, -1.0f, 0.0f, -1.0f },
        { 1.0f, 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, -1.0f, 0.0f }, { 1.0f, -1.0f, 1.0f, 0.0f }, { 1.0f, -1.0f, -1.0f, 0.0f },
        { -1.0f, 1.0f, 1.0f, 0.0f }, { -1.0f, 1.0f, -1.0f, 0.0f }, { -1.0f, -1.0f, 1.0f, 0.0f }, { -1.0f, -1.0f, -1.0f, 0.0f }
    };
    
    // For 3D, we define two orthogonal vectors in the desired rotation plane.
    // These vectors are based on the midpoints of the 12 edges of a cube,
    // they all rotate in their own plane and are never coincident or collinear.
    // A larger array of random vectors would also do the job, but these 12
    // (including 4 repeats to make the array length a power of two) work better.
    // They are not random, they are carefully chosen to represent a small
    // isotropic set of directions for any rotation angle.
    
    // a = sqrt(2)/sqrt(3) = 0.816496580
    
    static float s_gradient_3d_u[16][3] = {
        { 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 1.0f },
        { -1.0f, 0.0f, 1.0f }, { 0.0f, -1.0f, 1.0f },
        { 1.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, -1.0f },
        { -1.0f, 0.0f, -1.0f }, { 0.0f, -1.0f, -1.0f },
        { 0.81649658f, 0.81649658f, 0.81649658f }, { -0.81649658f, 0.81649658f, -0.81649658f },
        { -0.81649658f, -0.81649658f, 0.81649658f }, { 0.81649658f, -0.81649658f, -0.81649658f },
        { -0.81649658f, 0.81649658f, 0.81649658f }, { 0.81649658f, -0.81649658f, 0.81649658f },
        { 0.81649658f, -0.81649658f, -0.81649658f }, { -0.81649658f, 0.81649658f, -0.81649658f }
    };

    static float s_gradient_3d_v[16][3] = {
        { -0.81649658f, 0.81649658f, 0.81649658f }, { -0.81649658f, -0.81649658f, 0.81649658f },
        { 0.81649658f, -0.81649658f, 0.81649658f }, { 0.81649658f, 0.81649658f, 0.81649658f },
        { -0.81649658f, -0.81649658f, -0.81649658f }, { 0.81649658f, -0.81649658f, -0.81649658f },
        { 0.81649658f, 0.81649658f, -0.81649658f }, { -0.81649658f, 0.81649658f, -0.81649658f },
        { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f },
        { -1.0f, 1.0f, 0.0f }, { -1.0f, -1.0f, 0.0f },
        { 1.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, -1.0f }, { 0.0f, -1.0f, -1.0f }
    };
    
    // Helper functions to compute gradients-dot-residual vectors (1D to 4D)
    // Note that these generate gradients of more than unit length. To make
    // a close match with the value range of classic Perlin noise, the final
    // noise values need to be rescaled to fit nicely within [-1,1].
    // (The simplex noise functions as such also have different scaling.)
    // Note also that these noise functions are the most practical and useful
    // signed version of Perlin noise. To return values according to the
    // RenderMan specification from the SL noise() and pnoise() functions,
    // the noise values need to be scaled and offset to [0,1], like this:
    // float SLnoise = (noise(x,y,z) + 1.0) * 0.5;

    inline float grad(int hash, float x)
    {
        int h = hash & 15;
        float grad = 1.0f + (h & 7); // Gradient value 1.0, 2.0, ..., 8.0
        if (h&8) grad = -grad; // Set a random sign for the gradient
        return (grad * x); // Multiply the gradient with the distance
    }
    
    inline float grad(int hash, float x, float y)
    {
        int h = hash & 7; // Convert low 3 bits of hash code
        float u = h<4 ? x : y; // into 8 simple gradient directions,
        float v = h<4 ? y : x; // and compute the dot product with (x,y).
        return ((h&1)? -u : u) + ((h&2)? -2.0f*v : 2.0f*v);
    }
    
    inline float grad(int hash, float x, float y , float z)
    {
        int h = hash & 15; // Convert low 4 bits of hash code into 12 simple
        float u = h<8 ? x : y; // gradient directions, and compute dot product.
        float v = h<4 ? y : h==12||h==14 ? x : z; // Fix repeats at h = 12 to 15
        return ((h&1)? -u : u) + ((h&2)? -v : v);
    }
    
    inline float grad(int hash, float x, float y, float z, float t)
    {
        int h = hash & 31; // Convert low 5 bits of hash code into 32 simple
        float u = h < 24 ? x : y; // gradient directions, and compute dot product.
        float v = h < 16 ? y : z;
        float w = h < 8 ? z : t;
        return ((h & 1) ? -u : u) + ((h & 2) ? -v : v) + ((h & 4) ? -w : w);
    }
    
    // Helper functions to compute gradients in 1D to 4D and gradients-dot-residual vectors in 2D to 4D.
    inline void grad1(int hash, float *gx)
    {
        int h = hash & 15;
        *gx = 1.0f + (h & 7); // Gradient value is one of 1.0, 2.0, ..., 8.0
        if (h & 8) *gx = - *gx; // Make half of the gradients negative
    }
    
    inline void grad2(int hash, float *gx, float *gy)
    {
        int h = hash & 7;
        *gx = s_gradient_2_table[h][0];
        *gy = s_gradient_2_table[h][1];
    }
    
    inline void grad3(int hash, float *gx, float *gy, float *gz)
    {
        int h = hash & 15;
        *gx = s_gradient_3_table[h][0];
        *gy = s_gradient_3_table[h][1];
        *gz = s_gradient_3_table[h][2];
    }
    
    inline void grad4(int hash, float *gx, float *gy, float *gz, float *gw)
    {
        int h = hash & 31;
        *gx = s_gradient_4_table[h][0];
        *gy = s_gradient_4_table[h][1];
        *gz = s_gradient_4_table[h][2];
        *gw = s_gradient_4_table[h][3];
    }

    // Helper functions to compute rotated gradients and gradients-dot-residual vectors in 2D and 3D.
    inline void gradrot2(int hash, float sin_t, float cos_t, float *gx, float *gy)
    {
        int h = hash & 7;
        float gx0 = s_gradient_2_table[h][0];
        float gy0 = s_gradient_2_table[h][1];
        *gx = cos_t * gx0 - sin_t * gy0;
        *gy = sin_t * gx0 + cos_t * gy0;
    }
    
    inline void gradrot3(int hash, float sin_t, float cos_t, float *gx, float *gy, float *gz)
    {
        int h = hash & 15;
        float gux = s_gradient_3d_u[h][0];
        float guy = s_gradient_3d_u[h][1];
        float guz = s_gradient_3d_u[h][2];
        float gvx = s_gradient_3d_v[h][0];
        float gvy = s_gradient_3d_v[h][1];
        float gvz = s_gradient_3d_v[h][2];
        *gx = cos_t * gux + sin_t * gvx;
        *gy = cos_t * guy + sin_t * gvy;
        *gz = cos_t * guz + sin_t * gvz;
    }
    
    inline float graddotp2(float gx, float gy, float x, float y) 
    {
        return gx * x + gy * y;
    }
    
    inline float graddotp3(float gx, float gy, float gz, float x, float y, float z) 
    {
        return gx * x + gy * y + gz * z;
    }
    
}

inline void regenerate_permutation_table(std::mt19937 & gen)
{
    for (int i = 0; i < 256; i++)
    {
        auto num = std::uniform_int_distribution<int>(0, 255)(gen);
        impl::s_perm_table[i] = num;
        impl::s_perm_table[i + 256] = num;
    }
}
    
///////////////////////////////////
//   Dimensional Simplex Noise   //
///////////////////////////////////

inline float noise(float x)
{
    int i0 = fast_floor(x);
    int i1 = i0 + 1;
    float x0 = x - i0;
    float x1 = x0 - 1.0f;
    
    float n0, n1;
    
    float t0 = 1.0f - x0*x0;
    t0 *= t0;
    n0 = t0 * t0 * impl::grad(impl::s_perm_table[i0 & 0xff], x0);
    
    float t1 = 1.0f - x1*x1;
    t1 *= t1;
    n1 = t1 * t1 * impl::grad(impl::s_perm_table[i1 & 0xff], x1);

    // The maximum value of this noise is 8*(3/4)^4 = 2.53125
    // A factor of 0.395 would scale to fit exactly within [-1,1], but
    // we want to match PRMan's 1D noise, so we scale it down some more.
    return 0.25f * (n0 + n1);
}

inline float noise(const float2 & v)
{
    float n0, n1, n2; // Noise contributions from the three corners
    
    // Skew the input space to determine which simplex cell we're in
    float s = (v.x+v.y)*F2; // Hairy factor for 2D
    float xs = v.x + s;
    float ys = v.y + s;
    int i = fast_floor(xs);
    int j = fast_floor(ys);
    
    float t = (float)(i+j)*G2;
    float X0 = i-t; // Unskew the cell origin back to (x,y) space
    float Y0 = j-t;
    float x0 = v.x-X0; // The x,y distances from the cell origin
    float y0 = v.y-Y0;
    
    // For the 2D case, the simplex shape is an equilateral triangle.
    // Determine which simplex we are in.
    int i1, j1; // Offsets for second (middle) corner of simplex in (i,j) coords
    if (x0>y0) {i1=1; j1=0;} // lower triangle, XY order: (0,0)->(1,0)->(1,1)
    else {i1=0; j1=1;} // upper triangle, YX order: (0,0)->(0,1)->(1,1)
    
    // A step of (1,0) in (i,j) means a step of (1-c,-c) in (x,y), and
    // a step of (0,1) in (i,j) means a step of (-c,1-c) in (x,y), where
    // c = (3-sqrt(3))/6
    float x1 = x0 - i1 + G2; // Offsets for middle corner in (x,y) unskewed coords
    float y1 = y0 - j1 + G2;
    float x2 = x0 - 1.0f + 2.0f * G2; // Offsets for last corner in (x,y) unskewed coords
    float y2 = y0 - 1.0f + 2.0f * G2;
    
    // Wrap the integer indices at 256, to avoid indexing impl::s_perm_table[] out of bounds
    int ii = i & 0xff;
    int jj = j & 0xff;
    
    // Calculate the contribution from the three corners
    float t0 = 0.5f - x0*x0-y0*y0;
    if (t0 < 0.0f) n0 = 0.0f;
    else 
    {
        t0 *= t0;
        n0 = t0 * t0 * impl::grad(impl::s_perm_table[ii+impl::s_perm_table[jj]], x0, y0);
    }
    
    float t1 = 0.5f - x1*x1-y1*y1;
    if (t1 < 0.0f) n1 = 0.0f;
    else 
    {
        t1 *= t1;
        n1 = t1 * t1 * impl::grad(impl::s_perm_table[ii+i1+impl::s_perm_table[jj+j1]], x1, y1);
    }
    
    float t2 = 0.5f - x2*x2-y2*y2;
    if (t2 < 0.0f) n2 = 0.0f;
    else 
    {
        t2 *= t2;
        n2 = t2 * t2 * impl::grad(impl::s_perm_table[ii+1+impl::s_perm_table[jj+1]], x2, y2);
    }
    
    // Add contributions from each corner to get the final noise value.
    // The result is scaled to return values in the interval [-1,1].
    return 40.0f * (n0 + n1 + n2); // TODO: The scale factor is preliminary!
}

inline float noise(const float3 & v)
{
    float n0, n1, n2, n3; // Noise contributions from the four corners
    
    // Skew the input space to determine which simplex cell we're in
    float s = (v.x+v.y+v.z) * F3; // Very nice and simple skew factor for 3D
    float xs = v.x+s;
    float ys = v.y+s;
    float zs = v.z+s;
    int i = fast_floor(xs);
    int j = fast_floor(ys);
    int k = fast_floor(zs);
    
    float t = (float)(i+j+k) * G3;
    float X0 = i-t; // Unskew the cell origin back to (x,y,z) space
    float Y0 = j-t;
    float Z0 = k-t;
    float x0 = v.x-X0; // The x,y,z distances from the cell origin
    float y0 = v.y-Y0;
    float z0 = v.z-Z0;
    
    // For the 3D case, the simplex shape is a slightly irregular tetrahedron.
    // Determine which simplex we are in.
    int i1, j1, k1; // Offsets for second corner of simplex in (i,j,k) coords
    int i2, j2, k2; // Offsets for third corner of simplex in (i,j,k) coords
    
    if (x0>=y0) 
    {
        if (y0>=z0) { i1=1; j1=0; k1=0; i2=1; j2=1; k2=0; } // X Y Z order
        else if (x0>=z0) { i1=1; j1=0; k1=0; i2=1; j2=0; k2=1; } // X Z Y order
        else { i1=0; j1=0; k1=1; i2=1; j2=0; k2=1; } // Z X Y order
    }
    else 
    {
        if (y0<z0) { i1=0; j1=0; k1=1; i2=0; j2=1; k2=1; } // Z Y X order
        else if (x0<z0) { i1=0; j1=1; k1=0; i2=0; j2=1; k2=1; } // Y Z X order
        else { i1=0; j1=1; k1=0; i2=1; j2=1; k2=0; } // Y X Z order
    }
    
    // A step of (1,0,0) in (i,j,k) means a step of (1-c,-c,-c) in (x,y,z),
    // a step of (0,1,0) in (i,j,k) means a step of (-c,1-c,-c) in (x,y,z), and
    // a step of (0,0,1) in (i,j,k) means a step of (-c,-c,1-c) in (x,y,z), where
    // c = 1/6.
    
    float x1 = x0 - i1 + G3; // Offsets for second corner in (x,y,z) coords
    float y1 = y0 - j1 + G3;
    float z1 = z0 - k1 + G3;
    float x2 = x0 - i2 + 2.0f * G3; // Offsets for third corner in (x,y,z) coords
    float y2 = y0 - j2 + 2.0f * G3;
    float z2 = z0 - k2 + 2.0f * G3;
    float x3 = x0 - 1.0f + 3.0f * G3; // Offsets for last corner in (x,y,z) coords
    float y3 = y0 - 1.0f + 3.0f * G3;
    float z3 = z0 - 1.0f + 3.0f * G3;
    
    // Wrap the integer indices at 256, to avoid indexing impl::s_perm_table[] out of bounds
    int ii = i & 0xff;
    int jj = j & 0xff;
    int kk = k & 0xff;
    
    // Calculate the contribution from the four corners
    float t0 = 0.6f - x0*x0 - y0*y0 - z0*z0;
    if (t0 < 0.0f) n0 = 0.0f;
    else 
    {
        t0 *= t0;
        n0 = t0 * t0 * impl::grad(impl::s_perm_table[ii+impl::s_perm_table[jj+impl::s_perm_table[kk]]], x0, y0, z0);
    }
    
    float t1 = 0.6f - x1*x1 - y1*y1 - z1*z1;
    if (t1 < 0.0f) n1 = 0.0f;
    else 
    {
        t1 *= t1;
        n1 = t1 * t1 * impl::grad(impl::s_perm_table[ii+i1+impl::s_perm_table[jj+j1+impl::s_perm_table[kk+k1]]], x1, y1, z1);
    }
    
    float t2 = 0.6f - x2*x2 - y2*y2 - z2*z2;
    if (t2 < 0.0f) n2 = 0.0f;
    else 
    {
        t2 *= t2;
        n2 = t2 * t2 * impl::grad(impl::s_perm_table[ii+i2+impl::s_perm_table[jj+j2+impl::s_perm_table[kk+k2]]], x2, y2, z2);
    }
    
    float t3 = 0.6f - x3*x3 - y3*y3 - z3*z3;
    if (t3<0.0f) n3 = 0.0f;
    else 
    {
        t3 *= t3;
        n3 = t3 * t3 * impl::grad(impl::s_perm_table[ii+1+impl::s_perm_table[jj+1+impl::s_perm_table[kk+1]]], x3, y3, z3);
    }
    
    // Add contributions from each corner to get the final noise value.
    // The result is scaled to stay just inside [-1,1]
    return 32.0f * (n0 + n1 + n2 + n3); // TODO: The scale factor is preliminary!
}

namespace impl 
{
    static uint8_t s_simplex_table[64][4] = 
    {
        {0,1,2,3},{0,1,3,2},{0,0,0,0},{0,2,3,1},{0,0,0,0},{0,0,0,0},{0,0,0,0},{1,2,3,0},
        {0,2,1,3},{0,0,0,0},{0,3,1,2},{0,3,2,1},{0,0,0,0},{0,0,0,0},{0,0,0,0},{1,3,2,0},
        {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
        {1,2,0,3},{0,0,0,0},{1,3,0,2},{0,0,0,0},{0,0,0,0},{0,0,0,0},{2,3,0,1},{2,3,1,0},
        {1,0,2,3},{1,0,3,2},{0,0,0,0},{0,0,0,0},{0,0,0,0},{2,0,3,1},{0,0,0,0},{2,1,3,0},
        {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
        {2,0,1,3},{0,0,0,0},{0,0,0,0},{0,0,0,0},{3,0,1,2},{3,0,2,1},{0,0,0,0},{3,1,2,0},
        {2,1,0,3},{0,0,0,0},{0,0,0,0},{0,0,0,0},{3,1,0,2},{0,0,0,0},{3,2,0,1},{3,2,1,0}
    };
}

inline float noise(const float4 & v)
{
    float n0, n1, n2, n3, n4; // Noise contributions from the five corners
    
    // Skew the (x,y,z,w) space to determine which cell of 24 simplices we're in
    float s = (v.x + v.y + v.z + v.w) * F4; // Factor for 4D skewing
    float xs = v.x + s;
    float ys = v.y + s;
    float zs = v.z + s;
    float ws = v.w + s;
    int i = fast_floor(xs);
    int j = fast_floor(ys);
    int k = fast_floor(zs);
    int l = fast_floor(ws);
    
    float t = (i + j + k + l) * G4; // Factor for 4D unskewing
    float X0 = i - t; // Unskew the cell origin back to (x,y,z,w) space
    float Y0 = j - t;
    float Z0 = k - t;
    float W0 = l - t;
    
    float x0 = v.x - X0;  // The x,y,z,w distances from the cell origin
    float y0 = v.y - Y0;
    float z0 = v.z - Z0;
    float w0 = v.w - W0;
    
    // For the 4D case, the simplex is a 4D shape I won't even try to describe.
    // To find out which of the 24 possible simplices we're in, we need to
    // determine the magnitude ordering of x0, y0, z0 and w0.
    // The method below is a good way of finding the ordering of x,y,z,w and
    // then find the correct traversal order for the simplex weíre in.
    // First, six pair-wise comparisons are performed between each possible pair
    // of the four coordinates, and the results are used to add up binary bits
    // for an integer index.
    int c1 = (x0 > y0) ? 32 : 0;
    int c2 = (x0 > z0) ? 16 : 0;
    int c3 = (y0 > z0) ? 8 : 0;
    int c4 = (x0 > w0) ? 4 : 0;
    int c5 = (y0 > w0) ? 2 : 0;
    int c6 = (z0 > w0) ? 1 : 0;
    int c = c1 + c2 + c3 + c4 + c5 + c6;
    
    int i1, j1, k1, l1; // The integer offsets for the second simplex corner
    int i2, j2, k2, l2; // The integer offsets for the third simplex corner
    int i3, j3, k3, l3; // The integer offsets for the fourth simplex corner
    
    // impl::s_simplex_table[c] is a 4-vector with the numbers 0, 1, 2 and 3 in some order.
    // Many values of c will never occur, since e.g. x>y>z>w makes x<z, y<w and x<w
    // impossible. Only the 24 indices which have non-zero entries make any sense.
    // We use a thresholding to set the coordinates in turn from the largest magnitude.
    // The number 3 in the "simplex" array is at the position of the largest coordinate.
    i1 = impl::s_simplex_table[c][0]>=3 ? 1 : 0;
    j1 = impl::s_simplex_table[c][1]>=3 ? 1 : 0;
    k1 = impl::s_simplex_table[c][2]>=3 ? 1 : 0;
    l1 = impl::s_simplex_table[c][3]>=3 ? 1 : 0;

    // The number 2 in the "simplex" array is at the second largest coordinate.
    i2 = impl::s_simplex_table[c][0]>=2 ? 1 : 0;
    j2 = impl::s_simplex_table[c][1]>=2 ? 1 : 0;
    k2 = impl::s_simplex_table[c][2]>=2 ? 1 : 0;
    l2 = impl::s_simplex_table[c][3]>=2 ? 1 : 0;

    // The number 1 in the "simplex" array is at the second smallest coordinate.
    i3 = impl::s_simplex_table[c][0]>=1 ? 1 : 0;
    j3 = impl::s_simplex_table[c][1]>=1 ? 1 : 0;
    k3 = impl::s_simplex_table[c][2]>=1 ? 1 : 0;
    l3 = impl::s_simplex_table[c][3]>=1 ? 1 : 0;
    
    // The fifth corner has all coordinate offsets = 1, so no need to look that up.
    
    float x1 = x0 - i1 + G4; // Offsets for second corner in (x,y,z,w) coords
    float y1 = y0 - j1 + G4;
    float z1 = z0 - k1 + G4;
    float w1 = w0 - l1 + G4;
    float x2 = x0 - i2 + 2.0f*G4; // Offsets for third corner in (x,y,z,w) coords
    float y2 = y0 - j2 + 2.0f*G4;
    float z2 = z0 - k2 + 2.0f*G4;
    float w2 = w0 - l2 + 2.0f*G4;
    float x3 = x0 - i3 + 3.0f*G4; // Offsets for fourth corner in (x,y,z,w) coords
    float y3 = y0 - j3 + 3.0f*G4;
    float z3 = z0 - k3 + 3.0f*G4;
    float w3 = w0 - l3 + 3.0f*G4;
    float x4 = x0 - 1.0f + 4.0f*G4; // Offsets for last corner in (x,y,z,w) coords
    float y4 = y0 - 1.0f + 4.0f*G4;
    float z4 = z0 - 1.0f + 4.0f*G4;
    float w4 = w0 - 1.0f + 4.0f*G4;
    
    // Wrap the integer indices at 256, to avoid indexing impl::s_perm_table[] out of bounds
    int ii = i & 0xff;
    int jj = j & 0xff;
    int kk = k & 0xff;
    int ll = l & 0xff;
    
    // Calculate the contribution from the five corners
    float t0 = 0.6f - x0*x0 - y0*y0 - z0*z0 - w0*w0;
    if (t0 < 0.0f) n0 = 0.0f;
    else 
    {
        t0 *= t0;
        n0 = t0 * t0 * impl::grad(impl::s_perm_table[ii+impl::s_perm_table[jj+impl::s_perm_table[kk+impl::s_perm_table[ll]]]], x0, y0, z0, w0);
    }
    
    float t1 = 0.6f - x1*x1 - y1*y1 - z1*z1 - w1*w1;
    if (t1 < 0.0f) n1 = 0.0f;
    else 
    {
        t1 *= t1;
        n1 = t1 * t1 * impl::grad(impl::s_perm_table[ii+i1+impl::s_perm_table[jj+j1+impl::s_perm_table[kk+k1+impl::s_perm_table[ll+l1]]]], x1, y1, z1, w1);
    }
    
    float t2 = 0.6f - x2*x2 - y2*y2 - z2*z2 - w2*w2;
    if (t2 < 0.0f) n2 = 0.0f;
    else 
    {
        t2 *= t2;
        n2 = t2 * t2 * impl::grad(impl::s_perm_table[ii+i2+impl::s_perm_table[jj+j2+impl::s_perm_table[kk+k2+impl::s_perm_table[ll+l2]]]], x2, y2, z2, w2);
    }
    
    float t3 = 0.6f - x3*x3 - y3*y3 - z3*z3 - w3*w3;
    if (t3 < 0.0f) n3 = 0.0f;
    else 
    {
        t3 *= t3;
        n3 = t3 * t3 * impl::grad(impl::s_perm_table[ii+i3+impl::s_perm_table[jj+j3+impl::s_perm_table[kk+k3+impl::s_perm_table[ll+l3]]]], x3, y3, z3, w3);
    }
    
    float t4 = 0.6f - x4*x4 - y4*y4 - z4*z4 - w4*w4;
    if (t4 < 0.0f) n4 = 0.0f;
    else 
    {
        t4 *= t4;
        n4 = t4 * t4 * impl::grad(impl::s_perm_table[ii+1+impl::s_perm_table[jj+1+impl::s_perm_table[kk+1+impl::s_perm_table[ll+1]]]], x4, y4, z4, w4);
    }
    
    // Sum up and scale the result to cover the range [-1,1]
    return 27.0f * (n0 + n1 + n2 + n3 + n4); // TODO: The scale factor is preliminary!
}

//////////////////////////////
//   Ridged Simplex Noise   //
//////////////////////////////

namespace impl 
{
    template<typename T>
    inline float compute_ridge_noise(const T & input) { return 1.0f - std::abs(noise(input)); }
}

inline float noise_ridged(float x)
{
    return impl::compute_ridge_noise(x);
}

inline float noise_ridged(const float2 & v)
{
    return impl::compute_ridge_noise(v);
}

inline float noise_ridged(const float3 & v)
{
    return impl::compute_ridge_noise(v);
}

inline float noise_ridged(const float4 & v)
{
    return impl::compute_ridge_noise(v);
}

////////////////////////////////////////////////
//   Simplex Noise Via Analytical Derivative  //
////////////////////////////////////////////////

inline float2 noise_deriv(float x)
{
    int i0 = fast_floor(x);
    int i1 = i0 + 1;
    float x0 = x - i0;
    float x1 = x0 - 1.0f;
    
    float gx0, gx1;
    float n0, n1;
    float t20, t40, t21, t41;
    
    float x20 = x0*x0;
    float t0 = 1.0f - x20;
    t20 = t0 * t0;
    t40 = t20 * t20;
    impl::grad1(impl::s_perm_table[i0 & 0xff], &gx0);
    n0 = t40 * gx0 * x0;
    
    float x21 = x1*x1;
    float t1 = 1.0f - x21;
    t21 = t1 * t1;
    t41 = t21 * t21;
    impl::grad1(impl::s_perm_table[i1 & 0xff], &gx1);
    n1 = t41 * gx1 * x1;
    
    // Compute derivative according to:
    // dnoise_dx = -8.0f * t20 * t0 * x0 * (gx0 * x0) + t40 * gx0;
    // dnoise_dx += -8.0f * t21 * t1 * x1 * (gx1 * x1) + t41 * gx1;
    
    float dnoise_dx = t20 * t0 * gx0 * x20;
    dnoise_dx += t21 * t1 * gx1 * x21;
    dnoise_dx *= -8.0f;
    dnoise_dx += t40 * gx0 + t41 * gx1;
    dnoise_dx *= 0.25f; // Scale derivative to match the noise scaling
    
    // The maximum value of this noise is 8*(3/4)^4 = 2.53125
    // A factor of 0.395 would scale to fit exactly within [-1,1], but
    // to better match classic Perlin noise, we scale it down some more.
    #ifdef SIMPLEX_DERIVATIVES_RESCALE
        return float2(0.3961965135f * (n0 + n1), dnoise_dx);
    #else
        return float2(0.25f * (n0 + n1), dnoise_dx);
    #endif
}

inline float3 noise_deriv(const float2 & v)
{
    float n0, n1, n2; // Noise contributions from the three corners
    
    // Skew the input space to determine which simplex cell we're in
    float s = (v.x+v.y)*F2; // Hairy factor for 2D
    float xs = v.x + s;
    float ys = v.y + s;
    int i = fast_floor(xs);
    int j = fast_floor(ys);
    
    float t = (float)(i+j)*G2;
    float X0 = i-t; // Unskew the cell origin back to (x,y) space
    float Y0 = j-t;
    float x0 = v.x-X0; // The x,y distances from the cell origin
    float y0 = v.y-Y0;
    
    // For the 2D case, the simplex shape is an equilateral triangle.
    // Determine which simplex we are in.
    int i1, j1; // Offsets for second (middle) corner of simplex in (i,j) coords
    if (x0>y0) {i1=1; j1=0;} // lower triangle, XY order: (0,0)->(1,0)->(1,1)
    else 
    {i1=0; j1=1;}      // upper triangle, YX order: (0,0)->(0,1)->(1,1)
    
    // A step of (1,0) in (i,j) means a step of (1-c,-c) in (x,y), and
    // a step of (0,1) in (i,j) means a step of (-c,1-c) in (x,y), where
    // c = (3-sqrt(3))/6
    
    float x1 = x0 - i1 + G2; // Offsets for middle corner in (x,y) unskewed coords
    float y1 = y0 - j1 + G2;
    float x2 = x0 - 1.0f + 2.0f * G2; // Offsets for last corner in (x,y) unskewed coords
    float y2 = y0 - 1.0f + 2.0f * G2;
    
    // Wrap the integer indices at 256, to avoid indexing impl::s_perm_table[] out of bounds
    int ii = i & 0xff;
    int jj = j & 0xff;
    
    float gx0, gy0, gx1, gy1, gx2, gy2; // Gradients at simplex corners

    // Calculate the contribution from the three corners
    float t0 = 0.5f - x0 * x0 - y0 * y0;
    float t20, t40;
    if (t0 < 0.0f) t40 = t20 = t0 = n0 = gx0 = gy0 = 0.0f; // No influence
    else 
    {
        impl::grad2(impl::s_perm_table[ii + impl::s_perm_table[jj]], &gx0, &gy0);
        t20 = t0 * t0;
        t40 = t20 * t20;
        n0 = t40 * (gx0 * x0 + gy0 * y0);
    }
    
    float t1 = 0.5f - x1 * x1 - y1 * y1;
    float t21, t41;
    if (t1 < 0.0f) t21 = t41 = t1 = n1 = gx1 = gy1 = 0.0f; // No influence
    else 
    {
        impl::grad2(impl::s_perm_table[ii + i1 + impl::s_perm_table[jj + j1]], &gx1, &gy1);
        t21 = t1 * t1;
        t41 = t21 * t21;
        n1 = t41 * (gx1 * x1 + gy1 * y1);
    }
    
    float t2 = 0.5f - x2 * x2 - y2 * y2;
    float t22, t42;
    if (t2 < 0.0f) t42 = t22 = t2 = n2 = gx2 = gy2 = 0.0f; // No influence
    else 
    {
        impl::grad2(impl::s_perm_table[ii + 1 + impl::s_perm_table[jj + 1]], &gx2, &gy2);
        t22 = t2 * t2;
        t42 = t22 * t22;
        n2 = t42 * (gx2 * x2 + gy2 * y2);
    }
    
    float temp0 = t20 * t0 * (gx0* x0 + gy0 * y0);
    float dnoise_dx = temp0 * x0;
    float dnoise_dy = temp0 * y0;
    float temp1 = t21 * t1 * (gx1 * x1 + gy1 * y1);
    dnoise_dx += temp1 * x1;
    dnoise_dy += temp1 * y1;
    float temp2 = t22 * t2 * (gx2* x2 + gy2 * y2);
    dnoise_dx += temp2 * x2;
    dnoise_dy += temp2 * y2;
    dnoise_dx *= -8.0f;
    dnoise_dy *= -8.0f;
    dnoise_dx += t40 * gx0 + t41 * gx1 + t42 * gx2;
    dnoise_dy += t40 * gy0 + t41 * gy1 + t42 * gy2;
    dnoise_dx *= 40.0f; // Scale derivative to match the noise scaling
    dnoise_dy *= 40.0f;
    
    // Add contributions from each corner to get the final noise value.
    // The result is scaled to return values in the interval [-1,1].
#ifdef SIMPLEX_DERIVATIVES_RESCALE
    return float3(70.175438596f * (n0 + n1 + n2), dnoise_dx, dnoise_dy); // TODO: The scale factor is preliminary!
#else
    return float3(40.0f * (n0 + n1 + n2), dnoise_dx, dnoise_dy); // TODO: The scale factor is preliminary!
#endif
    
}

inline float4 noise_deriv(const float3 & v)
{
    float n0, n1, n2, n3; // Noise contributions from the four simplex corners
    float noise;          // Return value
    float gx0, gy0, gz0, gx1, gy1, gz1; // Gradients at simplex corners
    float gx2, gy2, gz2, gx3, gy3, gz3;
    
    // Skew the input space to determine which simplex cell we're in
    float s = (v.x+v.y+v.z) * F3; // Very nice and simple skew factor for 3D
    float xs = v.x+s;
    float ys = v.y+s;
    float zs = v.z+s;
    int i = fast_floor(xs);
    int j = fast_floor(ys);
    int k = fast_floor(zs);
    
    float t = (float)(i+j+k) * G3;
    float X0 = i-t; // Unskew the cell origin back to (x,y,z) space
    float Y0 = j-t;
    float Z0 = k-t;
    float x0 = v.x-X0; // The x,y,z distances from the cell origin
    float y0 = v.y-Y0;
    float z0 = v.z-Z0;
    
    // For the 3D case, the simplex shape is a slightly irregular tetrahedron.
    // Determine which simplex we are in.
    int i1, j1, k1; // Offsets for second corner of simplex in (i,j,k) coords
    int i2, j2, k2; // Offsets for third corner of simplex in (i,j,k) coords
    
    // TODO: This code would benefit from a backport from the GLSL version!
    if (x0>=y0)
    {
        if (y0>=z0) { i1=1; j1=0; k1=0; i2=1; j2=1; k2=0; } // X Y Z order
        else if (x0>=z0) { i1=1; j1=0; k1=0; i2=1; j2=0; k2=1; } // X Z Y order
        else  { i1=0; j1=0; k1=1; i2=1; j2=0; k2=1; } // Z X Y order
    }
    else 
    { // x0<y0
        if (y0<z0) { i1=0; j1=0; k1=1; i2=0; j2=1; k2=1; } // Z Y X order
        else if (x0<z0) { i1=0; j1=1; k1=0; i2=0; j2=1; k2=1; } // Y Z X order
        else  { i1=0; j1=1; k1=0; i2=1; j2=1; k2=0; } // Y X Z order
    }
    
    // A step of (1,0,0) in (i,j,k) means a step of (1-c,-c,-c) in (x,y,z),
    // a step of (0,1,0) in (i,j,k) means a step of (-c,1-c,-c) in (x,y,z), and
    // a step of (0,0,1) in (i,j,k) means a step of (-c,-c,1-c) in (x,y,z), where
    // c = 1/6.  
    
    float x1 = x0 - i1 + G3; // Offsets for second corner in (x,y,z) coords
    float y1 = y0 - j1 + G3;
    float z1 = z0 - k1 + G3;
    float x2 = x0 - i2 + 2.0f * G3; // Offsets for third corner in (x,y,z) coords
    float y2 = y0 - j2 + 2.0f * G3;
    float z2 = z0 - k2 + 2.0f * G3;
    float x3 = x0 - 1.0f + 3.0f * G3; // Offsets for last corner in (x,y,z) coords
    float y3 = y0 - 1.0f + 3.0f * G3;
    float z3 = z0 - 1.0f + 3.0f * G3;
    
    // Wrap the integer indices at 256, to avoid indexing impl::s_perm_table[] out of bounds
    int ii = i & 0xff;
    int jj = j & 0xff;
    int kk = k & 0xff;
    
    // Calculate the contribution from the four corners
    float t0 = 0.6f - x0*x0 - y0*y0 - z0*z0;
    float t20, t40;
    if (t0 < 0.0f) n0 = t0 = t20 = t40 = gx0 = gy0 = gz0 = 0.0f;
    else 
    {
        impl::grad3(impl::s_perm_table[ii + impl::s_perm_table[jj + impl::s_perm_table[kk]]], &gx0, &gy0, &gz0);
        t20 = t0 * t0;
        t40 = t20 * t20;
        n0 = t40 * (gx0 * x0 + gy0 * y0 + gz0 * z0);
    }
    
    float t1 = 0.6f - x1*x1 - y1*y1 - z1*z1;
    float t21, t41;
    if (t1 < 0.0f) n1 = t1 = t21 = t41 = gx1 = gy1 = gz1 = 0.0f;
    else 
    {
        impl::grad3(impl::s_perm_table[ii + i1 + impl::s_perm_table[jj + j1 + impl::s_perm_table[kk + k1]]], &gx1, &gy1, &gz1);
        t21 = t1 * t1;
        t41 = t21 * t21;
        n1 = t41 * (gx1 * x1 + gy1 * y1 + gz1 * z1);
    }
    
    float t2 = 0.6f - x2*x2 - y2*y2 - z2*z2;
    float t22, t42;
    if (t2 < 0.0f) n2 = t2 = t22 = t42 = gx2 = gy2 = gz2 = 0.0f;
    else 
    {
        impl::grad3(impl::s_perm_table[ii + i2 + impl::s_perm_table[jj + j2 + impl::s_perm_table[kk + k2]]], &gx2, &gy2, &gz2);
        t22 = t2 * t2;
        t42 = t22 * t22;
        n2 = t42 * (gx2 * x2 + gy2 * y2 + gz2 * z2);
    }
    
    float t3 = 0.6f - x3*x3 - y3*y3 - z3*z3;
    float t23, t43;
    if (t3 < 0.0f) n3 = t3 = t23 = t43 = gx3 = gy3 = gz3 = 0.0f;
    else 
    {
        impl::grad3(impl::s_perm_table[ii + 1 + impl::s_perm_table[jj + 1 + impl::s_perm_table[kk + 1]]], &gx3, &gy3, &gz3);
        t23 = t3 * t3;
        t43 = t23 * t23;
        n3 = t43 * (gx3 * x3 + gy3 * y3 + gz3 * z3);
    }
    
    //  Add contributions from each corner to get the final noise value.
    // The result is scaled to return values in the range [-1,1]
#ifdef SIMPLEX_DERIVATIVES_RESCALE
    noise = 34.525277436f * (n0 + n1 + n2 + n3);
#else
    noise = 28.0f * (n0 + n1 + n2 + n3);
#endif
    
    float temp0 = t20 * t0 * (gx0 * x0 + gy0 * y0 + gz0 * z0);
    float dnoise_dx = temp0 * x0;
    float dnoise_dy = temp0 * y0;
    float dnoise_dz = temp0 * z0;
    float temp1 = t21 * t1 * (gx1 * x1 + gy1 * y1 + gz1 * z1);
    dnoise_dx += temp1 * x1;
    dnoise_dy += temp1 * y1;
    dnoise_dz += temp1 * z1;
    float temp2 = t22 * t2 * (gx2 * x2 + gy2 * y2 + gz2 * z2);
    dnoise_dx += temp2 * x2;
    dnoise_dy += temp2 * y2;
    dnoise_dz += temp2 * z2;
    float temp3 = t23 * t3 * (gx3 * x3 + gy3 * y3 + gz3 * z3);
    dnoise_dx += temp3 * x3;
    dnoise_dy += temp3 * y3;
    dnoise_dz += temp3 * z3;
    dnoise_dx *= -8.0f;
    dnoise_dy *= -8.0f;
    dnoise_dz *= -8.0f;
    dnoise_dx += t40 * gx0 + t41 * gx1 + t42 * gx2 + t43 * gx3;
    dnoise_dy += t40 * gy0 + t41 * gy1 + t42 * gy2 + t43 * gy3;
    dnoise_dz += t40 * gz0 + t41 * gz1 + t42 * gz2 + t43 * gz3;
    dnoise_dx *= 28.0f; // Scale derivative to match the noise scaling
    dnoise_dy *= 28.0f;
    dnoise_dz *= 28.0f;
    
    return float4(noise, dnoise_dx, dnoise_dy, dnoise_dz);
}

inline std::array<float,5> noise_deriv(const float4 & v)
{
    float n0, n1, n2, n3, n4; // Noise contributions from the five corners
    float noise; // Return value
    float gx0, gy0, gz0, gw0, gx1, gy1, gz1, gw1; // Gradients at simplex corners
    float gx2, gy2, gz2, gw2, gx3, gy3, gz3, gw3, gx4, gy4, gz4, gw4;
    float t20, t21, t22, t23, t24;
    float t40, t41, t42, t43, t44;
    
    // Skew the (x,y,z,w) space to determine which cell of 24 simplices we're in
    float s = (v.x + v.y + v.z + v.w) * F4; // Factor for 4D skewing
    float xs = v.x + s;
    float ys = v.y + s;
    float zs = v.z + s;
    float ws = v.w + s;
    int i = fast_floor(xs);
    int j = fast_floor(ys);
    int k = fast_floor(zs);
    int l = fast_floor(ws);
    
    float t = (i + j + k + l) * G4; // Factor for 4D unskewing
    float X0 = i - t; // Unskew the cell origin back to (x,y,z,w) space
    float Y0 = j - t;
    float Z0 = k - t;
    float W0 = l - t;
    
    float x0 = v.x - X0;  // The x,y,z,w distances from the cell origin
    float y0 = v.y - Y0;
    float z0 = v.z - Z0;
    float w0 = v.w - W0;
    
    // For the 4D case, the simplex is a 4D shape I won't even try to describe.
    // To find out which of the 24 possible simplices we're in, we need to
    // determine the magnitude ordering of x0, y0, z0 and w0.
    // The method below is a reasonable way of finding the ordering of x,y,z,w
    // and then find the correct traversal order for the simplex weíre in.
    // First, six pair-wise comparisons are performed between each possible pair
    // of the four coordinates, and then the results are used to add up binary
    // bits for an integer index into a precomputed lookup table, impl::s_simplex_table[].
    int c1 = (x0 > y0) ? 32 : 0;
    int c2 = (x0 > z0) ? 16 : 0;
    int c3 = (y0 > z0) ? 8 : 0;
    int c4 = (x0 > w0) ? 4 : 0;
    int c5 = (y0 > w0) ? 2 : 0;
    int c6 = (z0 > w0) ? 1 : 0;
    int c = c1 | c2 | c3 | c4 | c5 | c6; // '|' is mostly faster than '+'
    
    int i1, j1, k1, l1; // The integer offsets for the second simplex corner
    int i2, j2, k2, l2; // The integer offsets for the third simplex corner
    int i3, j3, k3, l3; // The integer offsets for the fourth simplex corner
    
    // impl::s_simplex_table[c] is a 4-vector with the numbers 0, 1, 2 and 3 in some order.
    // Many values of c will never occur, since e.g. x>y>z>w makes x<z, y<w and x<w
    // impossible. Only the 24 indices which have non-zero entries make any sense.
    // We use a thresholding to set the coordinates in turn from the largest magnitude.
    // The number 3 in the "simplex" array is at the position of the largest coordinate.
    i1 = impl::s_simplex_table[c][0]>=3 ? 1 : 0;
    j1 = impl::s_simplex_table[c][1]>=3 ? 1 : 0;
    k1 = impl::s_simplex_table[c][2]>=3 ? 1 : 0;
    l1 = impl::s_simplex_table[c][3]>=3 ? 1 : 0;
    // The number 2 in the "simplex" array is at the second largest coordinate.
    i2 = impl::s_simplex_table[c][0]>=2 ? 1 : 0;
    j2 = impl::s_simplex_table[c][1]>=2 ? 1 : 0;
    k2 = impl::s_simplex_table[c][2]>=2 ? 1 : 0;
    l2 = impl::s_simplex_table[c][3]>=2 ? 1 : 0;
    // The number 1 in the "simplex" array is at the second smallest coordinate.
    i3 = impl::s_simplex_table[c][0]>=1 ? 1 : 0;
    j3 = impl::s_simplex_table[c][1]>=1 ? 1 : 0;
    k3 = impl::s_simplex_table[c][2]>=1 ? 1 : 0;
    l3 = impl::s_simplex_table[c][3]>=1 ? 1 : 0;
    // The fifth corner has all coordinate offsets = 1, so no need to look that up.
    
    float x1 = x0 - i1 + G4; // Offsets for second corner in (x,y,z,w) coords
    float y1 = y0 - j1 + G4;
    float z1 = z0 - k1 + G4;
    float w1 = w0 - l1 + G4;
    float x2 = x0 - i2 + 2.0f * G4; // Offsets for third corner in (x,y,z,w) coords
    float y2 = y0 - j2 + 2.0f * G4;
    float z2 = z0 - k2 + 2.0f * G4;
    float w2 = w0 - l2 + 2.0f * G4;
    float x3 = x0 - i3 + 3.0f * G4; // Offsets for fourth corner in (x,y,z,w) coords
    float y3 = y0 - j3 + 3.0f * G4;
    float z3 = z0 - k3 + 3.0f * G4;
    float w3 = w0 - l3 + 3.0f * G4;
    float x4 = x0 - 1.0f + 4.0f * G4; // Offsets for last corner in (x,y,z,w) coords
    float y4 = y0 - 1.0f + 4.0f * G4;
    float z4 = z0 - 1.0f + 4.0f * G4;
    float w4 = w0 - 1.0f + 4.0f * G4;
    
    // Wrap the integer indices at 256, to avoid indexing impl::s_perm_table[] out of bounds
    int ii = i & 0xff;
    int jj = j & 0xff;
    int kk = k & 0xff;
    int ll = l & 0xff;
    
    // Calculate the contribution from the five corners
    float t0 = 0.6f - x0*x0 - y0*y0 - z0*z0 - w0*w0;
    if (t0 < 0.0f) n0 = t0 = t20 = t40 = gx0 = gy0 = gz0 = gw0 = 0.0f;
    else 
    {
        t20 = t0 * t0;
        t40 = t20 * t20;
        impl::grad4(impl::s_perm_table[ii+impl::s_perm_table[jj+impl::s_perm_table[kk+impl::s_perm_table[ll]]]], &gx0, &gy0, &gz0, &gw0);
        n0 = t40 * (gx0 * x0 + gy0 * y0 + gz0 * z0 + gw0 * w0);
    }
    
    float t1 = 0.6f - x1*x1 - y1*y1 - z1*z1 - w1*w1;
    if (t1 < 0.0f) n1 = t1 = t21 = t41 = gx1 = gy1 = gz1 = gw1 = 0.0f;
    else 
    {
        t21 = t1 * t1;
        t41 = t21 * t21;
        impl::grad4(impl::s_perm_table[ii+i1+impl::s_perm_table[jj+j1+impl::s_perm_table[kk+k1+impl::s_perm_table[ll+l1]]]], &gx1, &gy1, &gz1, &gw1);
        n1 = t41 * (gx1 * x1 + gy1 * y1 + gz1 * z1 + gw1 * w1);
    }
    
    float t2 = 0.6f - x2*x2 - y2*y2 - z2*z2 - w2*w2;
    if (t2 < 0.0f) n2 = t2 = t22 = t42 = gx2 = gy2 = gz2 = gw2 = 0.0f;
    else 
    {
        t22 = t2 * t2;
        t42 = t22 * t22;
        impl::grad4(impl::s_perm_table[ii+i2+impl::s_perm_table[jj+j2+impl::s_perm_table[kk+k2+impl::s_perm_table[ll+l2]]]], &gx2, &gy2, &gz2, &gw2);
        n2 = t42 * (gx2 * x2 + gy2 * y2 + gz2 * z2 + gw2 * w2);
    }
    
    float t3 = 0.6f - x3*x3 - y3*y3 - z3*z3 - w3*w3;
    if (t3 < 0.0f) n3 = t3 = t23 = t43 = gx3 = gy3 = gz3 = gw3 = 0.0f;
    else 
    {
        t23 = t3 * t3;
        t43 = t23 * t23;
        impl::grad4(impl::s_perm_table[ii+i3+impl::s_perm_table[jj+j3+impl::s_perm_table[kk+k3+impl::s_perm_table[ll+l3]]]], &gx3, &gy3, &gz3, &gw3);
        n3 = t43 * (gx3 * x3 + gy3 * y3 + gz3 * z3 + gw3 * w3);
    }
    
    float t4 = 0.6f - x4*x4 - y4*y4 - z4*z4 - w4*w4;
    if (t4 < 0.0f) n4 = t4 = t24 = t44 = gx4 = gy4 = gz4 = gw4 = 0.0f;
    else 
    {
        t24 = t4 * t4;
        t44 = t24 * t24;
        impl::grad4(impl::s_perm_table[ii+1+impl::s_perm_table[jj+1+impl::s_perm_table[kk+1+impl::s_perm_table[ll+1]]]], &gx4, &gy4, &gz4, &gw4);
        n4 = t44 * (gx4 * x4 + gy4 * y4 + gz4 * z4 + gw4 * w4);
    }
    
    // Sum up and scale the result to cover the range [-1,1]
    noise = 27.0f * (n0 + n1 + n2 + n3 + n4); // TODO: The scale factor is preliminary!
    
    float temp0 = t20 * t0 * (gx0 * x0 + gy0 * y0 + gz0 * z0 + gw0 * w0);
    float dnoise_dx = temp0 * x0;
    float dnoise_dy = temp0 * y0;
    float dnoise_dz = temp0 * z0;
    float dnoise_dw = temp0 * w0;
    float temp1 = t21 * t1 * (gx1 * x1 + gy1 * y1 + gz1 * z1 + gw1 * w1);
    dnoise_dx += temp1 * x1;
    dnoise_dy += temp1 * y1;
    dnoise_dz += temp1 * z1;
    dnoise_dw += temp1 * w1;
    float temp2 = t22 * t2 * (gx2 * x2 + gy2 * y2 + gz2 * z2 + gw2 * w2);
    dnoise_dx += temp2 * x2;
    dnoise_dy += temp2 * y2;
    dnoise_dz += temp2 * z2;
    dnoise_dw += temp2 * w2;
    float temp3 = t23 * t3 * (gx3 * x3 + gy3 * y3 + gz3 * z3 + gw3 * w3);
    dnoise_dx += temp3 * x3;
    dnoise_dy += temp3 * y3;
    dnoise_dz += temp3 * z3;
    dnoise_dw += temp3 * w3;
    float temp4 = t24 * t4 * (gx4 * x4 + gy4 * y4 + gz4 * z4 + gw4 * w4);
    dnoise_dx += temp4 * x4;
    dnoise_dy += temp4 * y4;
    dnoise_dz += temp4 * z4;
    dnoise_dw += temp4 * w4;
    dnoise_dx *= -8.0f;
    dnoise_dy *= -8.0f;
    dnoise_dz *= -8.0f;
    dnoise_dw *= -8.0f;
    dnoise_dx += t40 * gx0 + t41 * gx1 + t42 * gx2 + t43 * gx3 + t44 * gx4;
    dnoise_dy += t40 * gy0 + t41 * gy1 + t42 * gy2 + t43 * gy3 + t44 * gy4;
    dnoise_dz += t40 * gz0 + t41 * gz1 + t42 * gz2 + t43 * gz3 + t44 * gz4;
    dnoise_dw += t40 * gw0 + t41 * gw1 + t42 * gw2 + t43 * gw3 + t44 * gw4;
    
    dnoise_dx *= 28.0f; // Scale derivative to match the noise scaling
    dnoise_dy *= 28.0f;
    dnoise_dz *= 28.0f;
    dnoise_dw *= 28.0f;
    
    return { noise, dnoise_dx, dnoise_dy, dnoise_dz, dnoise_dw };
}
    
/////////////////////////////////////////
//   2D Simplex Worley/Cellular Noise  //
/////////////////////////////////////////

inline float noise_worley(const float2 & v)
{
    float2 p = floor(v);
    float2 f = fract(v);
    
    float res = 8.0;
    for (int j=-1; j<=1; j++) 
    {
        for (int i=-1; i<=1; i++) 
        {
            float2 b = float2(i, j);
            float2  r = float2(b) - f + (noise::noise(p + b) * 0.5f + 0.5f);
            float d = dot(r, r);
            res = min(res, d);
        }
    }
    return sqrt(res);
}

inline float noise_worley(const float3 & v)
{
    float3 p = floor(v);
    float3 f = fract(v);
    
    float res = 8.0;
    for (int k=-1; k<=1; k++) 
    {
        for (int j=-1; j<=1; j++) 
        {
            for (int i=-1; i<=1; i++) 
            {
                float3 b = float3(i, j, k);
                float3 r = float3(b) - f + (noise::noise(p + b) * 0.5f + 0.5f);
                float d = dot(r, r);
                res = min(res, d);
            }
        }
    }
    return sqrt(res);
}

inline float noise_worley(const float2 & v, float falloff)
{
    auto p = floor(v);
    float2 f = fract(v);
    
    float res = 0.0f;
    for (int j=-1; j<=1; j++) 
    {
        for (int i=-1; i<=1; i++) 
        {
            auto b = float2(i, j);
            float2  r = float2(b) - f + (noise::noise(p + b) * 0.5f + 0.5f);
            float d = length(r);
            res += exp(-falloff*d);
        }
    }
    return -(1.0f / falloff) * std::log(res);
}

inline float noise_worley(const float3 & v, float falloff)
{
    float3 p = floor(v);
    float3 f = fract(v);
    
    float res = 0.0f;
    for (int k=-1; k<=1; k++) 
    {
        for (int j=-1; j<=1; j++) 
        {
            for (int i=-1; i<=1; i++) 
            {
                float3 b = float3(i, j, k);
                float3 r = float3(b) - f + (noise::noise(p + b) * 0.5f + 0.5f);
                float d = length(r);
                res += exp(-falloff*d);
            }
        }
    }
    return -(1.0f / falloff) * std::log(res);
}
    
/////////////////////////////////////////////////////////
//   2D/3D Simplex Flow Noise with Rotating Gradients  //
/////////////////////////////////////////////////////////

inline float noise_flow(const float2 & v, float angle)
{
    float n0, n1, n2; // Noise contributions from the three simplex corners
    float gx0, gy0, gx1, gy1, gx2, gy2; // Gradients at simplex corners
    float sin_t, cos_t; // Sine and cosine for the gradient rotation angle
    sin_t = sin(angle);
    cos_t = cos(angle);
    
    // Skew the input space to determine which simplex cell we're in
    float s = (v.x + v.y) * F2; // Hairy factor for 2D
    float xs = v.x + s;
    float ys = v.y + s;
    int i = fast_floor(xs);
    int j = fast_floor(ys);
    
    float t = (float) (i + j) * G2;
    float X0 = i - t; // Unskew the cell origin back to (x,y) space
    float Y0 = j - t;
    float x0 = v.x - X0; // The x,y distances from the cell origin
    float y0 = v.y - Y0;
    
    // For the 2D case, the simplex shape is an equilateral triangle.
    // Determine which simplex we are in.
    int i1, j1; // Offsets for second (middle) corner of simplex in (i,j) coords
    if (x0 > y0) { i1 = 1; j1 = 0; } // lower triangle, XY order: (0,0)->(1,0)->(1,1)
    else { i1 = 0; j1 = 1; } // upper triangle, YX order: (0,0)->(0,1)->(1,1)
    
    // A step of (1,0) in (i,j) means a step of (1-c,-c) in (x,y), and
    // a step of (0,1) in (i,j) means a step of (-c,1-c) in (x,y), where
    // c = (3-sqrt(3))/6  
    float x1 = x0 - i1 + G2; // Offsets for middle corner in (x,y) unskewed coords
    float y1 = y0 - j1 + G2;
    float x2 = x0 - 1.0f + 2.0f * G2; // Offsets for last corner in (x,y) unskewed coords
    float y2 = y0 - 1.0f + 2.0f * G2;
    
    // Wrap the integer indices at 256, to avoid indexing impl::s_perm_table[] out of bounds
    int ii = i & 0xff;
    int jj = j & 0xff;
    
    // Calculate the contribution from the three corners
    float t0 = 0.5f - x0 * x0 - y0 * y0;
    float t20, t40;
    if (t0 < 0.0f) t40 = t20 = t0 = n0 = gx0 = gy0 = 0.0f; // No influence
    else 
    {
        impl::gradrot2(impl::s_perm_table[ii + impl::s_perm_table[jj]], sin_t, cos_t, &gx0, &gy0);
        t20 = t0 * t0;
        t40 = t20 * t20;
        n0 = t40 * impl::graddotp2(gx0, gy0, x0, y0);
    }
    
    float t1 = 0.5f - x1 * x1 - y1 * y1;
    float t21, t41;
    if (t1 < 0.0f) t21 = t41 = t1 = n1 = gx1 = gy1 = 0.0f; // No influence
    else 
    {
        impl::gradrot2(impl::s_perm_table[ii + i1 + impl::s_perm_table[jj + j1]], sin_t, cos_t, &gx1, &gy1);
        t21 = t1 * t1;
        t41 = t21 * t21;
        n1 = t41 * impl::graddotp2(gx1, gy1, x1, y1);
    }
    
    float t2 = 0.5f - x2 * x2 - y2 * y2;
    float t22, t42;
    if (t2 < 0.0f) t42 = t22 = t2 = n2 = gx2 = gy2 = 0.0f; // No influence
    else 
    {
        impl::gradrot2(impl::s_perm_table[ii + 1 + impl::s_perm_table[jj + 1]], sin_t, cos_t, &gx2, &gy2);
        t22 = t2 * t2;
        t42 = t22 * t22;
        n2 = t42 * impl::graddotp2(gx2, gy2, x2, y2);
    }
    
    // Add contributions from each corner to get the final noise value.
    // The result is scaled to return values in the interval [-1,1].
    return 40.0f * (n0 + n1 + n2);
}

inline float noise_flow(const float3 & v, float angle)
{
    float n0, n1, n2, n3; // Noise contributions from the four simplex corners
    float gx0, gy0, gz0, gx1, gy1, gz1; // Gradients at simplex corners
    float gx2, gy2, gz2, gx3, gy3, gz3;
    float sin_t, cos_t; // Sine and cosine for the gradient rotation angle
    sin_t = sin(angle);
    cos_t = cos(angle);
    
    // Skew the input space to determine which simplex cell we're in
    float s = (v.x+v.y+v.z) * F3; // Very nice and simple skew factor for 3D
    float xs = v.x+s;
    float ys = v.y+s;
    float zs = v.z+s;
    int i = fast_floor(xs);
    int j = fast_floor(ys);
    int k = fast_floor(zs);
    
    float t = (float)(i+j+k) * G3;
    float X0 = i-t; // Unskew the cell origin back to (x,y,z) space
    float Y0 = j-t;
    float Z0 = k-t;
    float x0 = v.x-X0; // The x,y,z distances from the cell origin
    float y0 = v.y-Y0;
    float z0 = v.z-Z0;
    
    // For the 3D case, the simplex shape is a slightly irregular tetrahedron.
    // Determine which simplex we are in.
    int i1, j1, k1; // Offsets for second corner of simplex in (i,j,k) coords
    int i2, j2, k2; // Offsets for third corner of simplex in (i,j,k) coords
    
    if (x0>=y0)
    {
        if (y0>=z0) { i1=1; j1=0; k1=0; i2=1; j2=1; k2=0; }             // X Y Z order
        else if (x0>=z0) { i1=1; j1=0; k1=0; i2=1; j2=0; k2=1; }        // X Z Y order
        else { i1=0; j1=0; k1=1; i2=1; j2=0; k2=1; }                    // Z X Y order
    }
    else 
    {
        if (y0<z0) { i1=0; j1=0; k1=1; i2=0; j2=1; k2=1; }              // Z Y X order
        else if (x0<z0) { i1=0; j1=1; k1=0; i2=0; j2=1; k2=1; }         // Y Z X order
        else  { i1=0; j1=1; k1=0; i2=1; j2=1; k2=0; }                   // Y X Z order
    }
    
    // A step of (1,0,0) in (i,j,k) means a step of (1-c,-c,-c) in (x,y,z),
    // a step of (0,1,0) in (i,j,k) means a step of (-c,1-c,-c) in (x,y,z), and
    // a step of (0,0,1) in (i,j,k) means a step of (-c,-c,1-c) in (x,y,z), where
    // c = 1/6.  
    
    float x1 = x0 - i1 + G3; // Offsets for second corner in (x,y,z) coords
    float y1 = y0 - j1 + G3;
    float z1 = z0 - k1 + G3;
    float x2 = x0 - i2 + 2.0f * G3; // Offsets for third corner in (x,y,z) coords
    float y2 = y0 - j2 + 2.0f * G3;
    float z2 = z0 - k2 + 2.0f * G3;
    float x3 = x0 - 1.0f + 3.0f * G3; // Offsets for last corner in (x,y,z) coords
    float y3 = y0 - 1.0f + 3.0f * G3;
    float z3 = z0 - 1.0f + 3.0f * G3;
    
    // Wrap the integer indices at 256, to avoid indexing impl::s_perm_table[] out of bounds
    int ii = i & 0xff;
    int jj = j & 0xff;
    int kk = k & 0xff;
    
    // Calculate the contribution from the four corners
    float t0 = 0.6f - x0*x0 - y0*y0 - z0*z0;
    float t20, t40;
    if (t0 < 0.0f) n0 = t0 = t20 = t40 = gx0 = gy0 = gz0 = 0.0f;
    else 
    {
        impl::gradrot3(impl::s_perm_table[ii + impl::s_perm_table[jj + impl::s_perm_table[kk]]], sin_t, cos_t, &gx0, &gy0, &gz0);
        t20 = t0 * t0;
        t40 = t20 * t20;
        n0 = t40 * impl::graddotp3(gx0, gy0, gz0, x0, y0, z0);
    }
    
    float t1 = 0.6f - x1*x1 - y1*y1 - z1*z1;
    float t21, t41;
    if (t1 < 0.0f) n1 = t1 = t21 = t41 = gx1 = gy1 = gz1 = 0.0f;
    else 
    {
        impl::gradrot3(impl::s_perm_table[ii + i1 + impl::s_perm_table[jj + j1 + impl::s_perm_table[kk + k1]]], sin_t, cos_t, &gx1, &gy1, &gz1);
        t21 = t1 * t1;
        t41 = t21 * t21;
        n1 = t41 * impl::graddotp3(gx1, gy1, gz1, x1, y1, z1);
    }
    
    float t2 = 0.6f - x2*x2 - y2*y2 - z2*z2;
    float t22, t42;
    if (t2 < 0.0f) n2 = t2 = t22 = t42 = gx2 = gy2 = gz2 = 0.0f;
    else 
    {
        impl::gradrot3(impl::s_perm_table[ii + i2 + impl::s_perm_table[jj + j2 + impl::s_perm_table[kk + k2]]], sin_t, cos_t, &gx2, &gy2, &gz2);
        t22 = t2 * t2;
        t42 = t22 * t22;
        n2 = t42 * impl::graddotp3(gx2, gy2, gz2, x2, y2, z2);
    }
    
    float t3 = 0.6f - x3*x3 - y3*y3 - z3*z3;
    float t23, t43;
    if (t3 < 0.0f) n3 = t3 = t23 = t43 = gx3 = gy3 = gz3 = 0.0f;
    else 
    {
        impl::gradrot3(impl::s_perm_table[ii + 1 + impl::s_perm_table[jj + 1 + impl::s_perm_table[kk + 1]]], sin_t, cos_t, &gx3, &gy3, &gz3);
        t23 = t3 * t3;
        t43 = t23 * t23;
        n3 = t43 * impl::graddotp3(gx3, gy3, gz3, x3, y3, z3);
    }
    
    //  Add contributions from each corner to get the final noise value.
    // The result is scaled to return values in the range [-1,1]
    return 28.0f * (n0 + n1 + n2 + n3);
}

////////////////////////////////////////////////////////////
//   2D/3D Simplex Flow Noise Via Analytical Derivative   //
////////////////////////////////////////////////////////////

inline float3 noise_flow_deriv(const float2 & v, float angle)
{
    
    float n0, n1, n2; // Noise contributions from the three simplex corners
    float gx0, gy0, gx1, gy1, gx2, gy2; // Gradients at simplex corners
    float sin_t, cos_t; // Sine and cosine for the gradient rotation angle
    sin_t = sin(angle);
    cos_t = cos(angle);
    
    // Skew the input space to determine which simplex cell we're in
    float s = (v.x + v.y) * F2; // Hairy factor for 2D
    float xs = v.x + s;
    float ys = v.y + s;
    int i = fast_floor(xs);
    int j = fast_floor(ys);
    
    float t = (float) (i + j) * G2;
    float X0 = i - t; // Unskew the cell origin back to (x,y) space
    float Y0 = j - t;
    float x0 = v.x - X0; // The x,y distances from the cell origin
    float y0 = v.y - Y0;
    
    // For the 2D case, the simplex shape is an equilateral triangle.
    // Determine which simplex we are in.
    int i1, j1; // Offsets for second (middle) corner of simplex in (i,j) coords
    if (x0 > y0) { i1 = 1; j1 = 0; } // lower triangle, XY order: (0,0)->(1,0)->(1,1)
    else { i1 = 0; j1 = 1; }      // upper triangle, YX order: (0,0)->(0,1)->(1,1)
    
    // A step of (1,0) in (i,j) means a step of (1-c,-c) in (x,y), and
    // a step of (0,1) in (i,j) means a step of (-c,1-c) in (x,y), where
    // c = (3-sqrt(3))/6  
    float x1 = x0 - i1 + G2; // Offsets for middle corner in (x,y) unskewed coords
    float y1 = y0 - j1 + G2;
    float x2 = x0 - 1.0f + 2.0f * G2; // Offsets for last corner in (x,y) unskewed coords
    float y2 = y0 - 1.0f + 2.0f * G2;
    
    // Wrap the integer indices at 256, to avoid indexing impl::s_perm_table[] out of bounds
    int ii = i & 0xff;
    int jj = j & 0xff;
    
    // Calculate the contribution from the three corners
    float t0 = 0.5f - x0 * x0 - y0 * y0;
    float t20, t40;
    if (t0 < 0.0f) t40 = t20 = t0 = n0 = gx0 = gy0 = 0.0f; // No influence
    else 
    {
        impl::gradrot2(impl::s_perm_table[ii + impl::s_perm_table[jj]], sin_t, cos_t, &gx0, &gy0);
        t20 = t0 * t0;
        t40 = t20 * t20;
        n0 = t40 * impl::graddotp2(gx0, gy0, x0, y0);
    }
    
    float t1 = 0.5f - x1 * x1 - y1 * y1;
    float t21, t41;
    if (t1 < 0.0f) t21 = t41 = t1 = n1 = gx1 = gy1 = 0.0f; // No influence
    else 
    {
        impl::gradrot2(impl::s_perm_table[ii + i1 + impl::s_perm_table[jj + j1]], sin_t, cos_t, &gx1, &gy1);
        t21 = t1 * t1;
        t41 = t21 * t21;
        n1 = t41 * impl::graddotp2(gx1, gy1, x1, y1);
    }
    
    float t2 = 0.5f - x2 * x2 - y2 * y2;
    float t22, t42;
    if (t2 < 0.0f) t42 = t22 = t2 = n2 = gx2 = gy2 = 0.0f; // No influence
    else 
    {
        impl::gradrot2(impl::s_perm_table[ii + 1 + impl::s_perm_table[jj + 1]], sin_t, cos_t, &gx2, &gy2);
        t22 = t2 * t2;
        t42 = t22 * t22;
        n2 = t42 * impl::graddotp2(gx2, gy2, x2, y2);
    }
    
    // Add contributions from each corner to get the final noise value.
    // The result is scaled to return values in the interval [-1,1].
    float noise = 40.0f * (n0 + n1 + n2);
    
    // Compute derivative, if requested by supplying non-null pointers
    // for the last two arguments
    float dnoise_dx, dnoise_dy;
    
    float temp0 = t20 * t0 * impl::graddotp2(gx0, gy0, x0, y0);
    dnoise_dx = temp0 * x0;
    dnoise_dy = temp0 * y0;
    float temp1 = t21 * t1 * impl::graddotp2(gx1, gy1, x1, y1);
    dnoise_dx += temp1 * x1;
    dnoise_dy += temp1 * y1;
    float temp2 = t22 * t2 * impl::graddotp2(gx2, gy2, x2, y2);
    dnoise_dx += temp2 * x2;
    dnoise_dy += temp2 * y2;
    dnoise_dx *= -8.0f;
    dnoise_dy *= -8.0f;
    
    // This corrects a bug in the original implementation
    dnoise_dx += t40 * gx0 + t41 * gx1 + t42 * gx2;
    dnoise_dy += t40 * gy0 + t41 * gy1 + t42 * gy2;
    dnoise_dx *= 40.0f; // Scale derivative to match the noise scaling
    dnoise_dy *= 40.0f;
    
    return float3(noise, dnoise_dx, dnoise_dy);
}

inline float4 noise_flow_deriv(const float3 & v, float angle)
{
    float n0, n1, n2, n3; // Noise contributions from the four simplex corners
    float noise;
    float gx0, gy0, gz0, gx1, gy1, gz1; // Gradients at simplex corners
    float gx2, gy2, gz2, gx3, gy3, gz3;
    float sin_t, cos_t; // Sine and cosine for the gradient rotation angle
    sin_t = sin(angle);
    cos_t = cos(angle);
    
    // Skew the input space to determine which simplex cell we're in
    float s = (v.x+v.y+v.z) * F3; // Very nice and simple skew factor for 3D
    float xs = v.x+s;
    float ys = v.y+s;
    float zs = v.z+s;
    int i = fast_floor(xs);
    int j = fast_floor(ys);
    int k = fast_floor(zs);
    
    float t = (float)(i+j+k) * G3;
    float X0 = i-t; // Unskew the cell origin back to (x,y,z) space
    float Y0 = j-t;
    float Z0 = k-t;
    float x0 = v.x-X0; // The x,y,z distances from the cell origin
    float y0 = v.y-Y0;
    float z0 = v.z-Z0;
    
    // For the 3D case, the simplex shape is a slightly irregular tetrahedron.
    // Determine which simplex we are in.

    int i1, j1, k1; // Offsets for second corner of simplex in (i,j,k) coords
    int i2, j2, k2; // Offsets for third corner of simplex in (i,j,k) coords
    
    // TODO: This code would benefit from a backport from the GLSL version!
    if (x0>=y0) 
    {
        if (y0>=z0) { i1=1; j1=0; k1=0; i2=1; j2=1; k2=0; }             // X Y Z order
        else if (x0>=z0) { i1=1; j1=0; k1=0; i2=1; j2=0; k2=1; }        // X Z Y order
        else  { i1=0; j1=0; k1=1; i2=1; j2=0; k2=1; }                   // Z X Y order
    }
    else 
    {
        if (y0<z0) { i1=0; j1=0; k1=1; i2=0; j2=1; k2=1; }              // Z Y X order
        else if (x0<z0) { i1=0; j1=1; k1=0; i2=0; j2=1; k2=1; }         // Y Z X order
        else  { i1=0; j1=1; k1=0; i2=1; j2=1; k2=0; }                   // Y X Z order
    }
    
    // A step of (1,0,0) in (i,j,k) means a step of (1-c,-c,-c) in (x,y,z),
    // a step of (0,1,0) in (i,j,k) means a step of (-c,1-c,-c) in (x,y,z), and
    // a step of (0,0,1) in (i,j,k) means a step of (-c,-c,1-c) in (x,y,z), where
    // c = 1/6.  
    
    float x1 = x0 - i1 + G3; // Offsets for second corner in (x,y,z) coords
    float y1 = y0 - j1 + G3;
    float z1 = z0 - k1 + G3;
    float x2 = x0 - i2 + 2.0f * G3; // Offsets for third corner in (x,y,z) coords
    float y2 = y0 - j2 + 2.0f * G3;
    float z2 = z0 - k2 + 2.0f * G3;
    float x3 = x0 - 1.0f + 3.0f * G3; // Offsets for last corner in (x,y,z) coords
    float y3 = y0 - 1.0f + 3.0f * G3;
    float z3 = z0 - 1.0f + 3.0f * G3;
    
    // Wrap the integer indices at 256, to avoid indexing impl::s_perm_table[] out of bounds
    int ii = i & 0xff;
    int jj = j & 0xff;
    int kk = k & 0xff;
    
    // Calculate the contribution from the four corners
    float t0 = 0.6f - x0*x0 - y0*y0 - z0*z0;
    float t20, t40;
    if (t0 < 0.0f) n0 = t0 = t20 = t40 = gx0 = gy0 = gz0 = 0.0f;
    else 
    {
        impl::gradrot3(impl::s_perm_table[ii + impl::s_perm_table[jj + impl::s_perm_table[kk]]], sin_t, cos_t, &gx0, &gy0, &gz0);
        t20 = t0 * t0;
        t40 = t20 * t20;
        n0 = t40 * impl::graddotp3(gx0, gy0, gz0, x0, y0, z0);
    }
    
    float t1 = 0.6f - x1*x1 - y1*y1 - z1*z1;
    float t21, t41;
    if (t1 < 0.0f) n1 = t1 = t21 = t41 = gx1 = gy1 = gz1 = 0.0f;
    else 
    {
        impl::gradrot3(impl::s_perm_table[ii + i1 + impl::s_perm_table[jj + j1 + impl::s_perm_table[kk + k1]]], sin_t, cos_t, &gx1, &gy1, &gz1);
        t21 = t1 * t1;
        t41 = t21 * t21;
        n1 = t41 * impl::graddotp3(gx1, gy1, gz1, x1, y1, z1);
    }
    
    float t2 = 0.6f - x2*x2 - y2*y2 - z2*z2;
    float t22, t42;
    if (t2 < 0.0f) n2 = t2 = t22 = t42 = gx2 = gy2 = gz2 = 0.0f;
    else 
    {
        impl::gradrot3(impl::s_perm_table[ii + i2 + impl::s_perm_table[jj + j2 + impl::s_perm_table[kk + k2]]], sin_t, cos_t, &gx2, &gy2, &gz2);
        t22 = t2 * t2;
        t42 = t22 * t22;
        n2 = t42 * impl::graddotp3(gx2, gy2, gz2, x2, y2, z2);
    }
    
    float t3 = 0.6f - x3*x3 - y3*y3 - z3*z3;
    float t23, t43;
    if (t3 < 0.0f) n3 = t3 = t23 = t43 = gx3 = gy3 = gz3 = 0.0f;
    else 
    {
        impl::gradrot3(impl::s_perm_table[ii + 1 + impl::s_perm_table[jj + 1 + impl::s_perm_table[kk + 1]]], sin_t, cos_t, &gx3, &gy3, &gz3);
        t23 = t3 * t3;
        t43 = t23 * t23;
        n3 = t43 * impl::graddotp3(gx3, gy3, gz3, x3, y3, z3);
    }
    
    //  Add contributions from each corner to get the final noise value.
    // The result is scaled to return values in the range [-1,1]
    noise = 28.0f * (n0 + n1 + n2 + n3);
    
    // Compute derivative, if requested by supplying non-null pointers
    // for the last three arguments
    float dnoise_dx, dnoise_dy, dnoise_dz;
    
    float temp0 = t20 * t0 * impl::graddotp3(gx0, gy0, gz0, x0, y0, z0);
    dnoise_dx = temp0 * x0;
    dnoise_dy = temp0 * y0;
    dnoise_dz = temp0 * z0;

    float temp1 = t21 * t1 * impl::graddotp3(gx1, gy1, gz1, x1, y1, z1);
    dnoise_dx += temp1 * x1;
    dnoise_dy += temp1 * y1;
    dnoise_dz += temp1 * z1;

    float temp2 = t22 * t2 * impl::graddotp3(gx2, gy2, gz2, x2, y2, z2);
    dnoise_dx += temp2 * x2;
    dnoise_dy += temp2 * y2;
    dnoise_dz += temp2 * z2;

    float temp3 = t23 * t3 * impl::graddotp3(gx3, gy3, gz3, x3, y3, z3);
    dnoise_dx += temp3 * x3;
    dnoise_dy += temp3 * y3;
    dnoise_dz += temp3 * z3;
    dnoise_dx *= -8.0f;
    dnoise_dy *= -8.0f;
    dnoise_dz *= -8.0f;

    // This corrects a bug in the original implementation
    dnoise_dx += t40 * gx0 + t41 * gx1 + t42 * gx2 + t43 * gx3;
    dnoise_dy += t40 * gy0 + t41 * gy1 + t42 * gy2 + t43 * gy3;
    dnoise_dz += t40 * gz0 + t41 * gz1 + t42 * gz2 + t43 * gz3;
    dnoise_dx *= 28.0f; // Scale derivative to match the noise scaling
    dnoise_dy *= 28.0f;
    dnoise_dz *= 28.0f;
    
    return float4(noise, dnoise_dx, dnoise_dy, dnoise_dz);
}

//////////////////////////////////////////
//   Compute Curl of 2D Simplex Noise   //
//////////////////////////////////////////

inline float2 noise_curl(const float2 & v)
{
    const float3 derivative = noise_deriv(v);
    return float2(derivative.z, -derivative.y);
}

inline float2 noise_curl(const float2 & v, float t)
{
    const float3 derivative = noise_flow_deriv(v, t);
    return float2(derivative.z, -derivative.y);
}

inline float2 noise_curl(const float2 & v, uint8_t octaves, float lacunarity, float gain)
{
    const float3 derivative = noise_fb_deriv(v, octaves, lacunarity, gain);
    return float2(derivative.z, -derivative.y);
}

inline float3 noise_curl(const float3 & v)
{
    const float4 derivX = noise_deriv(v);
    const float4 derivY = noise_deriv(v + float3(123.456f, 789.012f, 345.678f));
    const float4 derivZ = noise_deriv(v + float3(901.234f, 567.891f, 234.567f));
    return float3(derivZ.z - derivY.w, derivX.w - derivZ.y, derivY.y - derivX.z);
}

inline float3 noise_curl(const float3 & v, float t)
{
    const float4 derivX = noise_flow_deriv(v, t);
    const float4 derivY = noise_flow_deriv(v + float3(123.456f, 789.012f, 345.678f), t);
    const float4 derivZ = noise_flow_deriv(v + float3(901.234f, 567.891f, 234.567f), t);
    return float3(derivZ.z - derivY.w, derivX.w - derivZ.y, derivY.y - derivX.z);
}

inline float3 noise_curl(const float3 & v, uint8_t octaves, float lacunarity, float gain)
{
    const float4 derivX = noise_fb_deriv(v, octaves, lacunarity, gain);
    const float4 derivY = noise_fb_deriv(v + float3(123.456f, 789.012f, 345.678f), octaves, lacunarity, gain);
    const float4 derivZ = noise_fb_deriv(v + float3(901.234f, 567.891f, 234.567f), octaves, lacunarity, gain);
    return float3(derivZ.z - derivY.w, derivX.w - derivZ.y, derivY.y - derivX.z);
}

inline float2 curl(const float2 & v, const std::function<float(const float2&)> &potential, float delta)
{
    const float2 deltaX = float2(delta, 0.0f);
    const float2 deltaY = float2(0.0f, delta);
    return float2(-(potential(v + deltaY) - potential(v - deltaY)),
                   (potential(v + deltaX) - potential(v - deltaX))) / (2.0f * delta);
}
    
inline float3 curl(const float3 & v, const std::function<float3(const float3&)> &potential, float delta)
{
    const float3 deltaX = float3(delta, 0.0f, 0.0f);
    const float3 deltaY = float3(0.0f, delta, 0.0f);
    const float3 deltaZ = float3(0.0f, 0.0f, delta);
    return float3((   (potential(v + deltaY).z - potential(v - deltaY).z)
                     -(potential(v + deltaZ).y - potential(v - deltaZ).y)),
                     ((potential(v + deltaZ).x - potential(v - deltaZ).x)
                     -(potential(v + deltaX).z - potential(v - deltaX).z)),
                     ((potential(v + deltaX).y - potential(v - deltaX).y)
                     -(potential(v + deltaY).x - potential(v - deltaY).x))) / (2.0f * delta);
}

/////////////////////////////////
//   Fractal Brownian Motion   //
/////////////////////////////////

namespace impl 
{
    template<typename T>
    inline float compute_fractal_brownian(const T & input, uint8_t octaves, float lacunarity, float gain)
    {
        float sum  = 0.0f;
        float freq = 1.0f;
        float amp = 0.5f;
        for (uint8_t i = 0; i < octaves; i++)
        {
            float n = noise(input * freq);
            sum += n*amp;
            freq *= lacunarity;
            amp *= gain;
        }
        return sum;
    }
}

inline float noise_fb(float x, uint8_t octaves, float lacunarity, float gain)
{
    return impl::compute_fractal_brownian(x, octaves, lacunarity, gain);
}

inline float noise_fb(const float2 & v, uint8_t octaves, float lacunarity, float gain)
{
    return impl::compute_fractal_brownian(v, octaves, lacunarity, gain);
}

inline float noise_fb(const float3 & v, uint8_t octaves, float lacunarity, float gain)
{
    return impl::compute_fractal_brownian(v, octaves, lacunarity, gain);
}

inline float noise_fb(const float4 & v, uint8_t octaves, float lacunarity, float gain)
{
    return impl::compute_fractal_brownian(v, octaves, lacunarity, gain);
}

///////////////////////////////////////////////////////////
//   Fractal Brownian Motion via Analytical Derivative   //
///////////////////////////////////////////////////////////

inline float2 noise_fb_deriv(float x, uint8_t octaves, float lacunarity, float gain)
{
    float2 sum = float2(0.0f);
    float freq = 1.0f;
    float amp = 0.5f;
    for (uint8_t i = 0; i < octaves; i++)
    {
        float2 n = noise_deriv(x * freq);
        sum += n*amp;
        freq *= lacunarity;
        amp *= gain;
    }
    return sum;
}

inline  float3 noise_fb_deriv(const float2 & v, uint8_t octaves, float lacunarity, float gain)
{
    float3 sum = float3(0.0f);
    float freq = 1.0f;
    float amp = 0.5f;
    
    for (uint8_t i = 0; i < octaves; i++)
    {
        float3 n = noise_deriv(v * freq);
        sum += n*amp;
        freq *= lacunarity;
        amp *= gain;
    }
    
    return sum;
}

inline float4 noise_fb_deriv(const float3 & v, uint8_t octaves, float lacunarity, float gain)
{
    float4 sum = float4(0.0f);
    float freq = 1.0f;
    float amp = 0.5f;
    for (uint8_t i = 0; i < octaves; i++)
    {
        float4 n = noise_deriv(v * freq);
        sum += n*amp;
        freq *= lacunarity;
        amp *= gain;
    }
    return sum;
}

inline std::array<float,5> noise_fb_deriv(const float4 & v, uint8_t octaves, float lacunarity, float gain)
{
    std::array<float,5> sum = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float freq  = 1.0f;
    float amp   = 0.5f;
    for (uint8_t i = 0; i < octaves; i++)
    {
        auto n = noise_deriv(v * freq);
        sum[0] += n[0]*amp;
        sum[1] += n[1]*amp;
        sum[2] += n[2]*amp;
        sum[3] += n[3]*amp;
        sum[4] += n[4]*amp;
        freq *= lacunarity;
        amp *= gain;
    }
    return sum;
}

////////////////////////////////////////
//   Ridged Multi-Fractal Noise Sum   //
////////////////////////////////////////

namespace impl 
{
    inline float ridge(float h, float offset)
    {
        h = offset - std::abs(h);
        return h * h;
    }
    
    template<typename T>
    inline float compute_ridged_multi_fractal(const T & input, float ridgeOffset, uint8_t octaves, float lacunarity, float gain)
    {
        float sum = 0;
        float freq = 1.0;
        float amp = 0.5;
        float prev = 1.0;
        for (uint8_t i = 0; i < octaves; i++)
        {
            float n = ridge(noise(input * freq), ridgeOffset);
            sum += n*amp*prev;
            prev = n;
            freq *= lacunarity;
            amp *= gain;
        }
        return sum;
    }
}

inline float noise_ridged_mf(float x, float ridgeOffset, uint8_t octaves, float lacunarity, float gain)
{
    return impl::compute_ridged_multi_fractal(x, ridgeOffset, octaves, lacunarity, gain);
}

inline float noise_ridged_mf(const float2 & v, float ridgeOffset, uint8_t octaves, float lacunarity, float gain)
{
    return impl::compute_ridged_multi_fractal(v, ridgeOffset, octaves, lacunarity, gain);
}

inline float noise_ridged_mf(const float3 & v, float ridgeOffset, uint8_t octaves, float lacunarity, float gain)
{
    return impl::compute_ridged_multi_fractal(v, ridgeOffset, octaves, lacunarity, gain);
}

inline float noise_ridged_mf(const float4 & v, float ridgeOffset, uint8_t octaves, float lacunarity, float gain)
{
    return impl::compute_ridged_multi_fractal(v, ridgeOffset, octaves, lacunarity, gain);
}

/////////////////////////////////////////////////////
//   Noise Fractal Variation Via Iñigo's Methods   //
/////////////////////////////////////////////////////

inline float noise_iq_fb(const float2 & v, uint8_t octaves, float lacunarity, float gain)
{
    float sum = 0.0;
    float amp = 0.5;
    float dx = 0.0;
    float dy = 0.0;
    float freq  = 1.0;
    for (uint8_t i = 0; i < octaves; i++) 
    {
        float3 d = noise_deriv(v * freq);
        dx += d.y;
        dy += d.y;
        sum += amp * d.x / (1.0 + dx*dx + dy*dy);
        freq *= lacunarity;
        amp *= gain;
    }
    return sum;
}

inline float noise_iq_fb(const float3 & v, uint8_t octaves, float lacunarity, float gain)
{
    float sum = 0.0;
    float amp = 0.5;
    float dx = 0.0;
    float dy = 0.0;
    float dz = 0.0;
    float freq  = 1.0;
    for (uint8_t i = 0; i < octaves; i++) 
    {
        float4 d = noise_deriv(v * freq);
        dx += d.y;
        dy += d.y;
        dz += d.z;
        sum += amp * d.x / (1.0 + dx*dx + dy*dy + d.z*d.z);
        freq *= lacunarity;
        amp *= gain;
    }
    return sum;
}

inline float noise_iq_fb(const float2 & v, uint8_t octaves, const float2x2 & mat, float gain)
{
    float sum = 0.0;
    float amp = 1.0;
    float2 pos = v;
    float2 noiseAccum = float2(0.0);
    for (uint8_t i = 0; i < octaves; i++)
    {
        float3 n = noise_deriv(pos);
        noiseAccum  += float2(n.y, n.z);
        sum += amp * n.x / (1.0 + dot(noiseAccum, noiseAccum));
        amp *= gain;
        pos = (mat * pos);
    }
    return sum;
}

} // end namespace noise

#pragma warning(pop)

#endif // end polymer_simplex_noise_hpp
