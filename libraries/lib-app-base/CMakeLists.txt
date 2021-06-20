cmake_minimum_required(VERSION 3.7)
project(polymer-app-base LANGUAGES CXX)

file(GLOB INCLUDE_FILES include/polymer-app-base/*.hpp include/polymer-app-base/wrappers/*.hpp)
file(GLOB SOURCE_FILES src/*.cpp)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(${PROJECT_NAME} STATIC ${INCLUDE_FILES} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party")

#target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/imgui")
#target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/nanovg")

# Library Dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC polymer-core)
target_link_libraries(${PROJECT_NAME} PUBLIC polymer-gfx-gl)
target_link_libraries(${PROJECT_NAME} PUBLIC imgui)
target_link_libraries(${PROJECT_NAME} PUBLIC nanovg)
target_link_libraries(${PROJECT_NAME} PUBLIC tinygizmo)
target_link_libraries(${PROJECT_NAME} PUBLIC glad)
target_link_libraries(${PROJECT_NAME} PUBLIC glfw)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "polymer-libraries")