cmake_minimum_required(VERSION 3.10...4.0)
project(tinygizmo LANGUAGES CXX)

file(GLOB INCLUDE_FILES *.hpp *.h)
file(GLOB SOURCE_FILES *.cpp *.c)

add_library(${PROJECT_NAME} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "dependencies")