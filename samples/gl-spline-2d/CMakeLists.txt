cmake_minimum_required(VERSION 3.10...4.0)
project(gl-spline-2d LANGUAGES CXX)

file(GLOB INCLUDE_FILES *.hpp)
file(GLOB SOURCE_FILES *.cpp)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

IF(MSVC)
   ADD_DEFINITIONS(/arch:AVX)
ENDIF(MSVC)

set(COMMON_FILES                              
)
source_group("Common" FILES ${COMMON_FILES})

# application
add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${COMMON_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
add_compile_definitions(NOMINMAX)

target_link_libraries(${PROJECT_NAME} "polymer-core")
target_link_libraries(${PROJECT_NAME} "polymer-app-base")
target_link_libraries(${PROJECT_NAME} "polymer-gfx-gl")
target_link_libraries(${PROJECT_NAME} "polymer-engine")
target_link_libraries(${PROJECT_NAME} "polymer-model-io")

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "samples")