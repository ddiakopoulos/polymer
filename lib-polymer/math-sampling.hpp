/*
 * File: math-sampling.hpp
 */

#pragma once 

#ifndef math_sampling_hpp
#define math_sampling_hpp

#include "math-common.hpp"

namespace polymer
{
    // https://www.sebastiansylvan.com/post/importancesampling/
    // https://en.wikipedia.org/wiki/Stochastic_universal_sampling
    template<typename T>
    std::vector<T> resample(const std::vector<T> & input, const std::vector<T> & weights, size_t out_n)
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
