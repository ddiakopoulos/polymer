/*
 * File: tests/lib-polymer-tests.cpp
 * This file implements test-cases for various built-in types. It is written with
 * inline documentation so that it can be used as a starter-guide and reference
 * to using basic Polymer types in a correct and idiomatic way.
 */

#include "lib-polymer.hpp"

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
    REQUIRE(quaternion.xyz == float3(0.f));

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
    const float4x4 rotation = make_rotation_matrix({ 0, 1, 0 }, (float) POLYMER_TAU);
    const float4x4 scale = make_scaling_matrix(0.5f);

    /// In this instance, the translation is applied to the rotation, before being applied to the scale. 
    /// This is commonly notated (m = t*r*s)
    const float4x4 combined_model_matrix_a = (translation * rotation * scale);
    const float4x4 matrix_a_equivalent = (translation * rotation) * scale;
    REQUIRE(combined_model_matrix_a == matrix_a_equivalent);

    const float4x4 r_matrix = (translation * rotation);
    REQUIRE(get_rotation_submatrix(r_matrix) == get_rotation_submatrix(rotation));

    /// todo - inverse, determinant, transpose
}

/// A pose is a rigid transform consisting of a float3 position and a float4 quaternion rotation.
/// Poses are composable using the * operator and invertable using `invert()`
TEST_CASE("poses, matrices, and transformations")
{
    const float4x4 matrix_xform = make_translation_matrix({ -8, 0, 8 });

    const transform pose_a = make_transform_from_matrix(matrix_xform);
    const transform pose_b = { quatf(0, 0, 0, 1), float3(-8, 0, 8) };

    REQUIRE(pose_a.matrix() == matrix_xform);
    REQUIRE(pose_a == pose_b);

    const transform pose_c = { make_rotation_quat_axis_angle({ 1, 0, 0 }, (float) POLYMER_TAU / 2.f),{ 5, 5, 5 } };
    const transform pose_d = {};
    const transform pose_e = make_transform_from_to(pose_c, pose_d);

    REQUIRE((pose_c.inverse() * pose_d) == pose_e);
}

TEST_CASE("projection matrices")
{
    const float width = 1024.f;
    const float height = 1024.f;
    const float aspectRatio = width / height;

    const float4x4 projectionMatrix = make_projection_matrix(to_radians(90.f), aspectRatio, 0.1f, 100.f);
    const float4x4 viewMatrix = linalg::identity;
    const float4x4 viewProjectionMatrix = projectionMatrix * viewMatrix;

    float outNear, outFar;
    near_far_clip_from_projection(projectionMatrix, outNear, outFar);

    REQUIRE(outNear == doctest::Approx(0.1f));
    REQUIRE(outFar == doctest::Approx(100.f));
    REQUIRE(vfov_from_projection(projectionMatrix) == doctest::Approx(to_radians(90.f)));
    REQUIRE(aspect_from_projection(projectionMatrix) == doctest::Approx(aspectRatio));

    const float4x4 orthographicProjectionMatrix = make_orthographic_matrix(0.0f, width, height, 0.0f, -1.0f, 1.0f);
}

TEST_CASE("glsl mirror functions")
{
    // Linear interpolation
    REQUIRE(mix(0.f, 1.f, 0.5f) == doctest::Approx(0.5f));
    REQUIRE(mix(0.f, 2.f, 0.5f) == doctest::Approx(1.0f));
    REQUIRE(mix(0.f, 2.f, 0.25f) == doctest::Approx(0.5f));

    /// todo - reflect, refact, faceforward
}

TEST_CASE("coordinate system conversions")
{
    const coord_system opengl{ coord_axis::right, coord_axis::up, coord_axis::back };
    const coord_system directx{ coord_axis::right, coord_axis::up, coord_axis::forward };

    const float4x4 ogl_to_directx = coordinate_system_from_to(opengl, directx);
    REQUIRE(opengl.is_right_handed());
    REQUIRE(directx.is_left_handed());
    REQUIRE(opengl.is_orthogonal());
    REQUIRE(directx.is_orthogonal());

    const coord_system bad{ coord_axis::right, coord_axis::up, coord_axis::up };
    REQUIRE(bad.is_orthogonal() == false);
}

TEST_CASE("axis-aligned bounding box (float, 2D)")
{
    const aabb_2d bounds = { {-1, -1}, {1, 1} };
    REQUIRE(bounds.size() == float2(2, 2));
    REQUIRE(bounds.center() == float2(0, 0));
    REQUIRE(bounds.area() == 4);
    REQUIRE(bounds.width() == 2);
    REQUIRE(bounds.height() == 2);
    REQUIRE(bounds.contains({ 0.5f, 0.5f }));
    REQUIRE_FALSE(bounds.contains({ 2.f, 0.5f }));

    const aabb_2d other = { { -3, -3 },{ -2, -2 } };
    REQUIRE_FALSE(bounds.intersects(other));

    const aabb_2d overlap = { { -0.5f, -0.5f },{ 0.5f, 0.5f } };
    REQUIRE(bounds.intersects(overlap));
}

TEST_CASE("axis-aligned bounding box (float, 3D)")
{
    aabb_3d bounds;
}

TEST_CASE("unifom random number generation")
{
    uniform_random_gen gen;

    // Generate a random float between 0 and 1 inclusive
    for (int i = 0; i < 32768; ++i)
    {
        const float rnd_flt = gen.random_float();
        REQUIRE(rnd_flt >= 0.f);
        REQUIRE(rnd_flt <= 1.f);
    }

    // Generate a "safe" random float
    for (int i = 0; i < 32768; ++i)
    {
        const float rnd_flt = gen.random_float_safe();
        REQUIRE(rnd_flt >= 0.001f);
        REQUIRE(rnd_flt <= 0.999f);
    }

    // Generate a float between 0 and two pi
    for (int i = 0; i < 32768; ++i)
    {
        const float rnd_flt = gen.random_float_sphere();
        REQUIRE(rnd_flt >= 0.f);
        REQUIRE(rnd_flt <= POLYMER_TAU);
    }

    // Generate a float between 0.5f and 1.0f
    for (int i = 0; i < 32768; ++i)
    {
        const float rnd_flt = gen.random_float(0.5f, 1.0f);
        REQUIRE(rnd_flt >= 0.5f);
        REQUIRE(rnd_flt <= 1.f);
    }

    // Generate an unsigned integer between 0 and 1024
    for (int i = 0; i < 32768; ++i)
    {
        const uint32_t rnd_int = gen.random_int(1024);
        REQUIRE(rnd_int >= 0);
        REQUIRE(rnd_int <= 1024);
    }
}

TEST_CASE("timers")
{
    // Manual timers are helpful to debug large sections of code
    manual_timer timer;
    timer.start();

    // Scoped timers log to std::cout when destructed
    {
        scoped_timer t("human readable description of timer");
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    timer.stop();

    std::cout << "Manual timer took: " << timer.get() << " ms" << std::endl;
    REQUIRE(timer.get() < 28);
    REQUIRE(timer.get() > 25);
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

    /// Note that this function is purely string based and does not resolve relative paths.
    const std::string path_d = { "../../../path/to/a/image.png" };
    REQUIRE(parent_directory_from_filepath(path_d) == "../../../path/to/a");
}

TEST_CASE("loading & saving binary files")
{
    struct arbitrary_pod
    {
        float x;
        uint32_t y;
        bool z;
    };

    arbitrary_pod outData, inData;

    outData.x = 1.f;
    outData.y = 555;
    outData.z = false;

    std::vector<uint8_t> outBuffer(sizeof(arbitrary_pod));
    std::memcpy(outBuffer.data(), &outData, sizeof(outData));

    write_file_binary("binary-sample.bin", outBuffer);

    auto inBuffer = read_file_binary("binary-sample.bin");
    std::memcpy(&inData, inBuffer.data(), sizeof(inData));

    REQUIRE(inData.x == 1.f);
    REQUIRE(inData.y == 555);
    REQUIRE(inData.z == false);

    REQUIRE_THROWS(read_file_binary("binary-sample-does-not-exist.bin"));
}