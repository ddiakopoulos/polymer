cmake_minimum_required(VERSION 3.7)
project(polymer-core LANGUAGES CXX)

file(GLOB INCLUDE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/polymer-core/math/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/polymer-core/queues/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/polymer-core/tools/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/polymer-core/util/*.hpp"
)
file(GLOB SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(${PROJECT_NAME} STATIC ${INCLUDE_FILES} ${SOURCE_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${INCLUDE_FILES} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party")

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "polymer-libraries")

# set(USE_STB ON)
# if(USE_STB)
# endif()