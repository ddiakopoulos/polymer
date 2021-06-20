cmake_minimum_required(VERSION 3.7)
project(polymer-model-io LANGUAGES CXX)

file(GLOB INCLUDE_FILES include/polymer-model-io/*.hpp)
file(GLOB SOURCE_FILES src/*.cpp)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

set(THIRD_PARTY_DEPS "${CMAKE_CURRENT_SOURCE_DIR}/third-party")

set(TINYPLY_FILES
    "${THIRD_PARTY_DEPS}/tinyply/tinyply.h"
)

set(TINYOBJ_FILES
    "${THIRD_PARTY_DEPS}/tinyobj/tiny_obj_loader.cc"
    "${THIRD_PARTY_DEPS}/tinyobj/tiny_obj_loader.h"
)

set(MESHOPTIMIZER_FILES
    "${THIRD_PARTY_DEPS}/meshoptimizer/vfetchoptimizer.cpp"
    "${THIRD_PARTY_DEPS}/meshoptimizer/vfetchanalyzer.cpp"
    "${THIRD_PARTY_DEPS}/meshoptimizer/vertexcodec.cpp"
    "${THIRD_PARTY_DEPS}/meshoptimizer/vcacheoptimizer.cpp"
    "${THIRD_PARTY_DEPS}/meshoptimizer/vcacheanalyzer.cpp"
    "${THIRD_PARTY_DEPS}/meshoptimizer/stripifier.cpp"
    "${THIRD_PARTY_DEPS}/meshoptimizer/simplifier.cpp"
    "${THIRD_PARTY_DEPS}/meshoptimizer/overdrawoptimizer.cpp"
    "${THIRD_PARTY_DEPS}/meshoptimizer/overdrawanalyzer.cpp"
    "${THIRD_PARTY_DEPS}/meshoptimizer/indexgenerator.cpp"
    "${THIRD_PARTY_DEPS}/meshoptimizer/indexcodec.cpp"
    "${THIRD_PARTY_DEPS}/meshoptimizer/meshoptimizer.h"
)

add_library(${PROJECT_NAME} STATIC ${INCLUDE_FILES} ${SOURCE_FILES} ${TINYPLY_FILES} ${TINYOBJ_FILES} ${MESHOPTIMIZER_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third-party")

# Library Dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC polymer-core)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "polymer-libraries")