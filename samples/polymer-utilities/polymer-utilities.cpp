/*
 * File: samples/polymer-utilities.cpp
 */

#include "index.hpp"

using namespace polymer;

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

TEST_CASE("string, path and filename manipulation")
{
    const std::string comma_delimited_str = { "this,is,the,polymer,framework" };
    auto result = polymer::split(comma_delimited_str, ',');
    std::vector<std::string> manual_result { "this", "is", "the", "polymer", "framework" };
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