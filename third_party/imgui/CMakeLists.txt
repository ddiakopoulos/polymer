cmake_minimum_required(VERSION 3.10...4.0)
project(imgui LANGUAGES CXX)

file(GLOB INCLUDE_FILES *.h)
file(GLOB SOURCE_FILES *.cpp)

add_library(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# for STB
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../")

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "dependencies")