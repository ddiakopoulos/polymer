#include "index.hpp"

using namespace polymer;

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

// `linalg.h` provides a well-tested variety of basic arithmetic types 
// following HLSL nomenclature. Polymer uses `linalg.h` to offer a
// a minimially-viable set of features to interact with modern graphics
// APIs. Polymer provides convenience functions converting to/from Eigen
// types for scientific computing (see other samples). 
TEST_CASE("linalg.h arithmetic types")
{
    /// Initializer list syntax
    float2 vec2 = { 1.f, 2.f };

    /// Constructor syntax
    float3 vec3(5, 6, 7);

    /// Polymer does not use a separate quaternion type
    float4 quaternion = { 0, 0, 0, 1 };
    REQUIRE(quaternion.w == 1.f);
    REQUIRE(quaternion.xyz() == float3(0.f));
}

TEST_CASE("linalg.h matrices & identities")
{
    /// Static globals are available for `Identity4x4`, `Identity3x3` and `Identity2x2`
    float4x4 model_matrix_a = Identity4x4;

    /// Matrices are stored in column-major order and must be initialized accordingly. 
    float4x4 model_matrix_b = { {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {3, 4, 5, 1} };

    /// Polymer provides ostream operators for all basic types to assist with debugging.
    /// Note that matrices are printed in row-major order for easier reading.
    std::cout << "model_matrix_b: " << model_matrix_b << std::endl;

    /// Array operator is overloaded to work on columns
    std::cout << "Fourth Column: " << model_matrix_b[3] << std::endl;
    REQUIRE(model_matrix_b[0] == float4(0.f));

    /// Specific row function
    std::cout << "First Row: " << model_matrix_b.row(0) << std::endl;
    REQUIRE(model_matrix_b.row(3) == float4(0, 0, 0, 1));
}

TEST_CASE("poses")
{

}

TEST_CASE("pose and matrix transformations")
{

}

TEST_CASE("projection matrices")
{

}

TEST_CASE("glsl mirror functions")
{

}

TEST_CASE("axis-aligned bounding box (2D)")
{

}

TEST_CASE("axis-aligned bounding box (3D)")
{

}

TEST_CASE("ring buffer")
{

}

TEST_CASE("unifom random number generation")
{

}

TEST_CASE("timers")
{

}

TEST_CASE("primitive (sphere)")
{

}

TEST_CASE("primitive (plane)")
{

}

TEST_CASE("primitive (lines & segments)")
{

}

TEST_CASE("primitive (frustum)")
{

}

TEST_CASE("simple raycasting")
{

}

TEST_CASE("polynomial root solvers")
{

}