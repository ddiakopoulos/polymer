#pragma once

#ifndef model_io_util_hpp
#define model_io_util_hpp

#include "math-core.hpp"
#include <unordered_map>
#include <nmmintrin.h>

struct unique_vertex
{
    avl::float3 position; avl::float2 texcoord; avl::float3 normal;
};

template <typename KeyType, typename ValueType>
struct unordered_map_generator
{
    struct Hash
    {
        uint32_t operator()(const KeyType & a) const
        {
            uint32_t digest = 0;
            for (size_t i = 0; i < sizeof(a); i++) digest = _mm_crc32_u8(digest, ((uint8_t*)&a)[i]);
            return digest;
        }
    };
    struct CompareEq
    {
        bool operator()(const KeyType & a, const KeyType & b) const
        {
            return !memcmp(&a, &b, sizeof(a));
        }
    };
    typedef std::unordered_map<KeyType, ValueType, Hash, CompareEq> Type;
};


#endif // end model_io_util_hpp
