/*
 * File: math-sampling.hpp
 */

#pragma once 

#ifndef math_sampling_hpp
#define math_sampling_hpp

#include "math-common.hpp"

namespace polymer
{

    inline float vdc_radical_inverse(uint32_t i)
    {
        i = (i & 0x55555555) << 1 | (i & 0xAAAAAAAA) >> 1;
        i = (i & 0x33333333) << 2 | (i & 0xCCCCCCCC) >> 2;
        i = (i & 0x0F0F0F0F) << 4 | (i & 0xF0F0F0F0) >> 4;
        i = (i & 0x00FF00FF) << 8 | (i & 0xFF00FF00) >> 8;
        i = (i << 16) | (i >> 16);
        return static_cast<float>(i) * 2.3283064365386963e-10f;
    }

    // Output coordinate is a sample in the +Z upper hemisphere
    inline float3 sample_hammersley_uniform(const uint32_t i, const uint32_t n)
    {
        float2 coord(static_cast<float>(i) / (static_cast<float>(n), vdc_radical_inverse(i)));
        const float phi = coord.y * 2.0f * POLYMER_PI;
        const float t = 1.0f - coord.x;
        const float s = sqrt(1.0f - t * t);
        return float3(s * cos(phi), s * sin(phi), t);
    }

    // Output coordinate is a sample in the +Z upper hemisphere
    inline float3 sample_hammersley_cosine(const uint32_t i, const uint32_t n)
    {
        float2 coord(static_cast<float>(i) / (static_cast<float>(n), vdc_radical_inverse(i)));
        const float phi = coord.y * 2.0f * POLYMER_PI;
        const float t = std::sqrt(1.0f - coord.x);
        const float s = std::sqrt(1.0f - t * t);
        return float3(s * std::cos(phi), s * std::sin(phi), t);
    }

    // https://www.sebastiansylvan.com/post/importancesampling/
    // https://en.wikipedia.org/wiki/Stochastic_universal_sampling
    template<typename T>
    std::vector<T> resample(const std::vector<T> & input, const std::vector<T> & weights, const size_t out_n)
    {
        std::vector<T> outputs(out_n);

        float sum_weights = 0.0f;
        for (size_t i = 0; i < input.size(); ++i) sum_weights += weights[i];

        const float sample_width = sum_weights / outputs.size();
        std::default_random_engine generator;
        std::uniform_real_distribution<float> rnd(0, sample_width);
        size_t output_sample_index = -1;
        float cumulative_weight = -rnd(generator);
        for (size_t i = 0; i < outputs.size(); ++i)
        {  
            const float dist = i * sample_width; // How far is this sample from the origin (minus offset)?  

            // Find which sample to output. Just walk up the samples until the sum
            // of the weights is > to the distance of the current sample
            while (dist >= cumulative_weight && output_sample_index + 1 < input.size())
            {
                cumulative_weight += weights[++output_sample_index];
            }
            outputs[i] = input[output_sample_index];
        }

        return outputs;
    }

} // end namespace polymer

#endif // end math_sampling_hpp
