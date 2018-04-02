#pragma once

#ifndef polymer_typeid_hpp
#define polymer_typeid_hpp

#include "index.hpp"

///////////////////////////////////////
//   Compile-Time Constant Hashing   //
///////////////////////////////////////

// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function

using HashValue = uint64_t;
constexpr HashValue hash_offset_basis = 0x84222325;
constexpr HashValue hash_prime_multiplier = 0x000001b3;

namespace detail
{
    // Helper function for performing the recursion for the compile time hash.
    template <std::size_t N>
    inline constexpr HashValue const_hash_fnv1a(const char(&str)[N], int start, HashValue hash)
    {
        // Perform the static_cast to uint64_t otherwise MSVC complains, about integral constant overflow (warning C4307).
        return (start == N || start == N - 1) ? hash : const_hash_fnv1a(str, start + 1, static_cast<HashValue>((hash ^ static_cast<unsigned char>(str[start])) * static_cast<uint64_t>(hash_prime_multiplier)));
    }
}  // namespace detail

   // Compile-time hash function.
template <std::size_t N>
inline constexpr HashValue const_hash_fnv1a(const char(&str)[N])
{
    return N <= 1 ? 0 : detail::const_hash_fnv1a(str, 0, hash_offset_basis);
}

HashValue hash_fnv1a(HashValue basis, const char * str, size_t len)
{
    if (str == nullptr || *str == 0 || len == 0) return 0;

    size_t count = 0;
    HashValue value = basis;
    while (*str && count < len)
    {
        value = (value ^ static_cast<unsigned char>(*str++)) * hash_prime_multiplier;
        ++count;
    }
    return value;
}

HashValue hash_fnv1a(const char * str, size_t len)
{
    return hash_fnv1a(hash_offset_basis, str, len);
}

HashValue hash_fnv1a(const char * str)
{
    const size_t npos = -1;
    return hash_fnv1a(str, npos);
}

// Functor for hashable types in STL containers.
struct Hasher
{
    template <class T>
    std::size_t operator()(const T & value) const { return hash_fnv1a(value); }
};

//////////////////////////////////////
//   Custom TypeId Implementation   //
//////////////////////////////////////

using TypeId = uint64_t;

template <typename T>
const char * get_typename()
{
    assert(false);
    /// If you get a compiler error that complains about TYPEID_NOT_SETUP, then you have not added 
    /// POLYMER_SETUP_TYPEID(T) such that the compilation unit being compiled uses it.
    return T::TYPEID_NOT_SETUP;
}

template <typename T>
TypeId get_typeid() { return T::TYPEID_NOT_SETUP; }

template <typename T>
struct typeid_traits { static constexpr bool kHasTypeId = false; };

#define POLYMER_SETUP_TYPEID(Type)             \
template <>                                    \
struct typeid_traits<Type> {                   \
    static constexpr bool kHasTypeId = true;   \
};                                             \
template <>                                    \
    inline const char* get_typename<Type>() {  \
    return #Type;                              \
}                                              \
template <>                                    \
    inline TypeId get_typeid<Type>() {         \
    return const_hash_fnv1a(#Type);            \
}                                          

/////////////////////////
//   TypeId Registry   //
/////////////////////////

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

// Setup polymer_typeids for a variety of common types

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
POLYMER_SETUP_TYPEID(Frustum);
POLYMER_SETUP_TYPEID(Pose);
POLYMER_SETUP_TYPEID(Bounds2D);
POLYMER_SETUP_TYPEID(Bounds3D);

#endif // end polymer_typeid_hpp