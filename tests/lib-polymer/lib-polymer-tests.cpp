/*
* File: tests/lib-polymer-tests.cpp
* This file implements test-cases for various built-in types. It is written with
* inline documentation so that it can be used as a starter-guide and reference
* to using basic Polymer types in a correct and idiomatic way.
*/

#include "index.hpp"

using namespace polymer;

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

/// `linalg.h` provides a well-tested variety of basic arithmetic types 
/// following HLSL nomenclature. Functionally, `linalg.h` offers a 
/// minimially-viable set of features to interact with modern graphics
/// APIs. Polymer provides convenience functions converting to/from Eigen
/// types for scientific computing (see other samples). 
TEST_CASE("linalg.h linear algebra basic types")
{
    /// Initializer list syntax
    const float2 vec2 = { 1.f, 2.f };

    /// Constructor syntax
    const float3 vec3(5, 6, 7);

    /// Polymer does not use a separate quaternion type
    const float4 quaternion = { 0, 0, 0, 1 };
    REQUIRE(quaternion.w == 1.f);
    REQUIRE(quaternion.xyz() == float3(0.f));

    const float3 a_vector = { 0.55f, 1.45f, 0.88f };
    const float3 normalized_vector = normalize(a_vector);

    std::cout << "normalized: " << normalized_vector << std::endl;
}

TEST_CASE("linalg.h matrices & identities")
{
    /// Static globals are available for `Identity4x4`, `Identity3x3` and `Identity2x2`
    const float4x4 model_matrix_a = Identity4x4;

    /// Matrices are stored in column-major order and must be initialized accordingly. 
    const float4x4 model_matrix_b = { { 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 3, 4, 5, 1 } };

    /// Polymer provides ostream operators for all basic types to assist with debugging.
    /// Note that matrices are printed in *row-major* order for easier reading.
    std::cout << "ostream operator example: " << model_matrix_b << std::endl;

    /// Array operator is overloaded to work on columns
    std::cout << "fourth column: " << model_matrix_b[3] << std::endl;
    REQUIRE(model_matrix_b[0] == float4(0.f));
    REQUIRE(model_matrix_b[3] == float4(3, 4, 5, 1));

    /// Specific accessor for rows
    std::cout << "first row: " << model_matrix_b.row(0) << std::endl;
    REQUIRE(model_matrix_b.row(3) == float4(0, 0, 0, 1));
    REQUIRE(model_matrix_b.row(0) == float4(0, 0, 0, 3));

    const float4x4 translation = make_translation_matrix({ 2, 2, 2 });
    const float4x4 rotation = make_rotation_matrix({ 0, 1, 0 }, POLYMER_TAU);
    const float4x4 scale = make_scaling_matrix(0.5f);

    /// >>>> operator * does NOT perform matrix multiplication <<<<
    /// Linalg provides a mul(...) function for performing left-handed matrix multiplies. 
    /// In this instance, the translation is applied to the rotation, before being applied to the scale. 
    /// This is commonly notated (m = t*r*s)
    const float4x4 combined_model_matrix_a = mul(translation, rotation, scale);
    const float4x4 matrix_a_equivalent = mul(mul(translation, rotation), scale);
    REQUIRE(combined_model_matrix_a == matrix_a_equivalent);

    const float4x4 r_matrix = mul(translation, rotation);
    REQUIRE(get_rotation_submatrix(r_matrix) == get_rotation_submatrix(rotation));

    /// todo - inverse, determinant, transpose
}

/// A pose is a rigid transform consisting of a float3 position and a float4 quaternion rotation.
/// Poses are composable using the * operator and invertable using `invert()`
TEST_CASE("poses, matrices, and transformations")
{
    const float4x4 matrix_xform = make_translation_matrix({ -8, 0, 8 });

    const Pose pose_a = make_pose_from_transform_matrix(matrix_xform);
    const Pose pose_b = { float4(0, 0, 0, 1), float3(-8, 0, 8) };

    REQUIRE(pose_a.matrix() == matrix_xform);
    REQUIRE(pose_a == pose_b);

    const Pose pose_c = { make_rotation_quat_axis_angle({ 1, 0, 0 }, POLYMER_TAU / 2),{ 5, 5, 5 } };
    const Pose pose_d = {};
    const Pose pose_e = make_pose_from_to(pose_c, pose_d);

    REQUIRE((pose_c.inverse() * pose_d) == pose_e);
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

TEST_CASE("string, path and filename manipulation")
{
    const std::string comma_delimited_str = { "this,is,the,polymer,framework" };
    auto result = polymer::split(comma_delimited_str, ',');
    std::vector<std::string> manual_result{ "this", "is", "the", "polymer", "framework" };
    REQUIRE(result == manual_result);

    const std::string path_a = { "a/relative/path/to/a/file.txt" };
    REQUIRE(get_extension(path_a) == "txt");

    const std::string path_b = { "../relative/../path/to/a/image.png" };
    REQUIRE(get_filename_with_extension(path_b) == "image.png");

    const std::string path_c = { "C:\\users\\dimitri\\profile.png" };
    REQUIRE(get_filename_without_extension(path_c) == "profile");

    /// Note that this function is purely string based and does not actually resolve relative paths.
    const std::string path_d = { "../../../path/to/a/image.png" };
    REQUIRE(parent_directory_from_filepath(path_d) == "../../../path/to/a");
}

TEST_CASE("loading & saving ascii files")
{

}

TEST_CASE("loading & saving binary files")
{

}