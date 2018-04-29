// Original Source: http://stereopsis.com/radix.html && http://codercorner.com/RadixSortRevisited.htm
// This is free and unencumbered software released into the public domain.

#pragma once

#ifndef radix_sort_hpp
#define radix_sort_hpp

#include <memory>
#include <stdint.h>
#include <algorithm>
#include <utility>
#include <vector>

namespace polymer
{
    class radix_sort
    {
        void float_flip(uint32_t & f) { int32_t mask = (int32_t(f) >> 31) | 0x80000000; f ^= mask; } // Warren Hunt, Manchor Ko
        void inverse_float_flip(uint32_t & f) { uint32_t mask = (int32_t(f ^ 0x80000000) >> 31) | 0x80000000; f ^= mask; } // Michael Herf

        constexpr static const uint32_t RADIX_LENGTH_BITS = 16;
        constexpr static const uint32_t HISTOGRAM_BUCKETS = (1 << RADIX_LENGTH_BITS);
        constexpr static const uint32_t BIT_MASK = (HISTOGRAM_BUCKETS - 1);

        template<typename T>
        void radix_impl(T * data, size_t size)
        {
            const uint32_t passes = (sizeof(T) * 8) % RADIX_LENGTH_BITS == 0 ? (sizeof(T) * 8) / RADIX_LENGTH_BITS : (sizeof(T) * 8) / RADIX_LENGTH_BITS + 1;
            static_assert(passes % 2 == 0, "must be even number");

            std::vector<size_t> histograms(passes * HISTOGRAM_BUCKETS);
            std::vector<T> result(size);

            // Build histograms in parallel
            for (size_t i = 0; i < size; i++)
            {
                T element = data[i];

                for (int r = 0; r < passes; r++)
                {
                    T pos = (element >> (r * RADIX_LENGTH_BITS)) & BIT_MASK;
                    histograms[r * HISTOGRAM_BUCKETS + pos] += 1;
                }
            }

            // Sum the histograms
            for (uint32_t r = 0; r < passes; r++)
            {
                size_t sum = 0;
                for (uint32_t i = 0; i < HISTOGRAM_BUCKETS; i++)
                {
                    size_t val = histograms[r * HISTOGRAM_BUCKETS + i];
                    histograms[r * HISTOGRAM_BUCKETS + i] = sum;
                    sum += val;
                }
            }

            T * src = data;
            T * dst = result.data();
            for (uint32_t r = 0; r < passes; r++)
            {
                for (size_t i = 0; i < size; i++)
                {
                    T element = src[i];
                    T pos = ((element >> (r * RADIX_LENGTH_BITS)) & BIT_MASK);

                    size_t & index = histograms[r * HISTOGRAM_BUCKETS + pos];
                    dst[index] = element;
                    index++;
                }

                std::swap(src, dst);
            }
        }

    public:

        template<typename T = typename std::enable_if<std::is_integral<T>::value, T>::type>
        void sort(T * data, size_t size)
        {
            radix_impl<T>(data, size);
        }

        void sort(float * data, size_t size)
        {
            for (size_t i = 0; i < size; i++) float_flip((uint32_t &)data[i]);
            radix_impl<uint32_t>((uint32_t *)data, size);
            for (size_t i = 0; i < size; i++) inverse_float_flip((uint32_t &)data[i]);
        }

    };

} // end namespace polymer

#endif // end radix_sort_hpp
