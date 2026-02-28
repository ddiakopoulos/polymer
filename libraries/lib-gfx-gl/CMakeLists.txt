cmake_minimum_required(VERSION 3.10...4.0)
project(polymer-gfx-gl LANGUAGES CXX)

file(GLOB_RECURSE INCLUDE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/polymer-gfx-gl/*.hpp")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(${PROJECT_NAME} INTERFACE ${INCLUDE_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${INCLUDE_FILES})
target_include_directories(${PROJECT_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_include_directories(${PROJECT_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party")
target_include_directories(${PROJECT_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/glad/include")
target_include_directories(${PROJECT_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/glfw3/include")
target_include_directories(${PROJECT_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../../lib-app-base/include")

# Library Dependencies
target_link_libraries(${PROJECT_NAME} INTERFACE polymer-core)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "polymer-libraries")