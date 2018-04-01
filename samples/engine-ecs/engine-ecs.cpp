#include "index.hpp"

using namespace polymer;

///////////////////////////////////////
//   Compile-Time Constant Hashing   //
///////////////////////////////////////

using HashValue = uint64_t;
constexpr HashValue hash_offset_basis = 0x84222325;
constexpr HashValue hash_prime_multiplier = 0x000001b3;

namespace detail 
{
    // Helper function for performing the recursion for the compile time hash.
    template <std::size_t N>
    inline constexpr HashValue ConstHash(const char(&str)[N], int start, HashValue hash) 
    {
        // Perform the static_cast to uint64_t otherwise MSVC complains, about integral constant overflow (warning C4307).
        return (start == N || start == N - 1) ? hash : ConstHash(str, start + 1, static_cast<HashValue>((hash ^ static_cast<unsigned char>(str[start])) * static_cast<uint64_t>(hash_prime_multiplier)));
    }
}  // namespace detail

// Compile-time hash function.
template <std::size_t N>
inline constexpr HashValue ConstHash(const char(&str)[N])
{
    return N <= 1 ? 0 : detail::ConstHash(str, 0, hash_offset_basis);
}

// Functor for hashable types in STL containers.
struct Hasher 
{
    template <class T>
    std::size_t operator()(const T& value) const { return Hash(value); }
};

//////////////////
//   Entities   //
//////////////////

// An Entity is an uniquely identifiable object in the Polymer runtime.
using Entity = uint64_t;
constexpr Entity NULL_ENTITY = 0;

//////////////////////////////////////
//   Custom TypeId Implementation   //
//////////////////////////////////////

using TypeId = unsigned int;

template <typename T>
const char * GetTypeName() 
{
    assert(false);
    // If you get a compiler error that complains about TYPEID_NOT_SETUP, then
    // you have not added POLYMER_SETUP_TYPEID(T) such that the compilation unit
    // being compiled uses it.
    return T::TYPEID_NOT_SETUP;
}

template <typename T>
TypeId GetTypeId() { return T::TYPEID_NOT_SETUP; }

template <typename T>
struct TypeIdTraits { static constexpr bool kHasTypeId = false; };

#define POLYMER_SETUP_TYPEID(Type)             \
template <>                                    \
struct TypeIdTraits<Type> {                    \
    static constexpr bool kHasTypeId = true;   \
};                                             \
template <>                                    \
    inline const char* GetTypeName<Type>() {   \
    return #Type;                              \
}                                              \
template <>                                    \
    inline TypeId GetTypeId<Type>() {          \
    return ConstHash(#Type);                   \
}                                          

 struct Registry
 {

 };

 struct System
 {

 };

 // Basic Types
 POLYMER_SETUP_TYPEID(bool);
 POLYMER_SETUP_TYPEID(int8_t);
 POLYMER_SETUP_TYPEID(uint8_t);
 POLYMER_SETUP_TYPEID(int16_t);
 POLYMER_SETUP_TYPEID(uint16_t);
 POLYMER_SETUP_TYPEID(int32_t);
 POLYMER_SETUP_TYPEID(uint32_t);
 POLYMER_SETUP_TYPEID(int64_t);
 POLYMER_SETUP_TYPEID(uint64_t);
 POLYMER_SETUP_TYPEID(float);
 POLYMER_SETUP_TYPEID(double);

 // Linalg + Polymer Types
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

IMPLEMENT_MAIN(int argc, char * argv[])
{
    return EXIT_SUCCESS;
}