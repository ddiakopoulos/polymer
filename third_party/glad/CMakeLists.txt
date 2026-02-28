cmake_minimum_required(VERSION 3.10...4.0)
project(glad LANGUAGES CXX)

file(GLOB INCLUDE_FILES include/glad/glad.h)
file(GLOB SOURCE_FILES src/glad.c)

add_library(${PROJECT_NAME} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "dependencies")