cmake_minimum_required(VERSION 3.7)
project(polymer-xr LANGUAGES CXX)

file(GLOB INCLUDE_FILES include/polymer-xr/*.hpp)
file(GLOB SOURCE_FILES src/*.cpp)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(${PROJECT_NAME} STATIC ${INCLUDE_FILES} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/openvr/include")

# Library Dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC polymer-core)
target_link_libraries(${PROJECT_NAME} PUBLIC polymer-engine)
target_link_libraries(${PROJECT_NAME} PUBLIC polymer-gfx-gl)
target_link_libraries(${PROJECT_NAME} PUBLIC polymer-app-base)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "polymer-libraries")