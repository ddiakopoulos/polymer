cmake_minimum_required(VERSION 3.7)
project(polymer)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options("$<$<CONFIG:DEBUG>:-DDEBUG>")

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

# Dependencies

if (NOT TARGET tinygizmo)
    add_subdirectory(${THIRD_PARTY_DIR}/tinygizmo tinygizmo)
endif()

if (NOT TARGET tinyexr)
    add_subdirectory(${THIRD_PARTY_DIR}/tinyexr tinyexr)
endif()

if (NOT TARGET nanovg)
    add_subdirectory(${THIRD_PARTY_DIR}/nanovg nanovg)
endif()

if (NOT TARGET glad)
    add_subdirectory(${THIRD_PARTY_DIR}/glad glad)
endif()

if (NOT TARGET imgui)
    add_subdirectory(${THIRD_PARTY_DIR}/imgui imgui)
endif()

if (NOT TARGET glfw)
    set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "Build the GLFW example programs")
    set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "Build the GLFW test programs")
    set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "Build the GLFW documentation")
    set(GLFW_INSTALL OFF CACHE INTERNAL "Generate installation target")
    add_subdirectory(${THIRD_PARTY_DIR}/glfw glfw)
endif()

if (NOT TARGET spdlog)
    add_subdirectory(${THIRD_PARTY_DIR}/spdlog spdlog)
endif()

# Libraries 

if (NOT TARGET polymer-engine)
    add_subdirectory(libraries/lib-engine polymer-engine)
endif()

if (NOT TARGET polymer-core)
    add_subdirectory(libraries/lib-polymer polymer-core)
endif()

if (NOT TARGET polymer-model-io)
    add_subdirectory(libraries/lib-model-io polymer-model-io)
endif()

if (NOT TARGET polymer-gfx-gl)
    add_subdirectory(libraries/lib-gfx-gl polymer-gfx-gl)
endif()

if (NOT TARGET polymer-xr)
    add_subdirectory(libraries/lib-xr polymer-xr)
endif()

if (NOT TARGET polymer-app-base)
    add_subdirectory(libraries/lib-app-base polymer-app-base)
endif()