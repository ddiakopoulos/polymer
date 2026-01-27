cmake_minimum_required(VERSION 3.7)
project(polymer-engine LANGUAGES CXX)

file(GLOB INCLUDE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/polymer-engine/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/polymer-engine/asset/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/polymer-engine/ecs/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/polymer-engine/physics/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/polymer-engine/renderer/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/polymer-engine/system/*.hpp"
)
file(GLOB SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(${PROJECT_NAME} STATIC ${INCLUDE_FILES} ${SOURCE_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${INCLUDE_FILES} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Library Dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC polymer-core)
target_link_libraries(${PROJECT_NAME} PUBLIC polymer-gfx-gl)
target_link_libraries(${PROJECT_NAME} PUBLIC polymer-model-io)
target_link_libraries(${PROJECT_NAME} PUBLIC tinyexr)
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "polymer-libraries")