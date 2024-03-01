/*
 * Based on: https://github.com/google/lullaby/blob/master/lullaby/util/typeid.h
 * Apache 2.0 License. Copyright 2017 Google Inc. All Rights Reserved.
 * See LICENSE file for full attribution information.
 */

#pragma once

#ifndef polymer_typeid_hpp
#define polymer_typeid_hpp

#include "polymer-core/lib-polymer.hpp"
#include <stdint.h>

namespace polymer
{
    ///////////////////////////////////////
    //   Compile-Time Constant Hashing   //
    ///////////////////////////////////////

    // https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
    using poly_hash_value = uint64_t;
    constexpr poly_hash_value hash_offset_basis = 0x84222325;
    constexpr poly_hash_value hash_prime_multiplier = 0x000001b3;

    namespace detail
    {
        // Helper function for performing the recursion for the compile time hash.
        template <std::size_t N>
        inline constexpr poly_hash_value const_hash(const char(&str)[N], int start, poly_hash_value hash)
        {
            // Perform the static_cast to uint64_t otherwise MSVC complains, about integral constant overflow (warning C4307).
            return (start == N || start == N - 1) ? hash : const_hash(str, start + 1, static_cast<poly_hash_value>((hash ^ static_cast<unsigned char>(str[start])) * static_cast<uint64_t>(hash_prime_multiplier)));
        }
    }  // namespace detail

    // Compile-time hash function.
    template <std::size_t N>
    inline constexpr poly_hash_value const_hash(const char(&str)[N])
    {
        return N <= 1 ? 0 : detail::const_hash(str, 0, hash_offset_basis);
    }

    inline poly_hash_value hash(poly_hash_value basis, const char * str, size_t len)
    {
        if (str == nullptr || *str == 0 || len == 0) return 0;

        size_t count = 0;
        poly_hash_value value = basis;
        while (*str && count < len)
        {
            value = (value ^ static_cast<unsigned char>(*str++)) * hash_prime_multiplier;
            ++count;
        }
        return value;
    }

    inline poly_hash_value hash(const char * str, size_t len)
    {
        return hash(hash_offset_basis, str, len);
    }

    inline poly_hash_value hash(const char * str)
    {
        const size_t npos = -1;
        return hash(str, npos);
    }

    // Functor for hashable types in STL containers.
    struct hasher
    {
        template <class T>
        std::size_t operator()(const T & value) const { return hash(value); }
    };

    //////////////////////////////////////
    //   Custom TypeId Implementation   //
    //////////////////////////////////////

    using poly_typeid = uint64_t;

    template <typename T>
    const char * get_typename()
    {
        assert(false);
        // If you get a compiler error that complains about TYPEID_NOT_SETUP, then you have not added 
        // POLYMER_SETUP_TYPEID(T) such that the compilation unit being compiled uses it.
        return T::TYPEID_NOT_SETUP;
    }

    template <typename T>
    poly_typeid get_typeid() { return T::TYPEID_NOT_SETUP; }

    inline poly_typeid get_typeid(const char * name) { return polymer::hash(name); }

    template <typename T>
    struct typeid_traits { static constexpr bool kHasTypeId = false; };

    #define POLYMER_SETUP_TYPEID(X)                     \
    template <>                                         \
    struct typeid_traits<X> {                           \
        static constexpr bool kHasTypeId = true;        \
    };                                                  \
    template <>                                         \
        inline const char* polymer::get_typename<X>() { \
        return #X;                                      \
    }                                                   \
    template <>                                         \
        inline poly_typeid get_typeid<X>() {            \
        return polymer::const_hash(#X);                 \
    }                                          

    //////////////////////////////
    //   poly_typeid registry   //
    //////////////////////////////

    // Used for SFINAE on other types we haven't setup yet (todo - stl containers)
    template <typename T>
    struct has_typename { static const bool value = true; };

    class type_name_generator
    {
        template <typename T>
        static auto generate_impl() -> typename std::enable_if<has_typename<T>::value, std::string>::type
        {
            return get_typename<T>();
        }
    public:
        /// Returns the name of the specified type |T|.
        template <typename T>
        static std::string generate() { return generate_impl<T>(); }
    };

    // Intrinsic Types
    POLYMER_SETUP_TYPEID(bool);
    POLYMER_SETUP_TYPEID(float);
    POLYMER_SETUP_TYPEID(double);
    POLYMER_SETUP_TYPEID(int8_t);
    POLYMER_SETUP_TYPEID(uint8_t);
    POLYMER_SETUP_TYPEID(int16_t);
    POLYMER_SETUP_TYPEID(uint16_t);
    POLYMER_SETUP_TYPEID(int32_t);
    POLYMER_SETUP_TYPEID(uint32_t);
    POLYMER_SETUP_TYPEID(int64_t);
    POLYMER_SETUP_TYPEID(uint64_t);
    POLYMER_SETUP_TYPEID(std::string);

    // Polymer Types
    POLYMER_SETUP_TYPEID(float2);
    POLYMER_SETUP_TYPEID(float3);
    POLYMER_SETUP_TYPEID(float4);
    POLYMER_SETUP_TYPEID(int2);
    POLYMER_SETUP_TYPEID(int3);
    POLYMER_SETUP_TYPEID(int4);
    POLYMER_SETUP_TYPEID(uint2);
    POLYMER_SETUP_TYPEID(uint3);
    POLYMER_SETUP_TYPEID(uint4);
    POLYMER_SETUP_TYPEID(float2x2);
    POLYMER_SETUP_TYPEID(float3x3);
    POLYMER_SETUP_TYPEID(float4x4);
    POLYMER_SETUP_TYPEID(aabb_2d);
    POLYMER_SETUP_TYPEID(aabb_3d);
    POLYMER_SETUP_TYPEID(frustum);
    POLYMER_SETUP_TYPEID(transform);

} // end namespace polymer

#endif // end polymer_typeid_hpp