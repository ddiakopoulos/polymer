cmake_minimum_required(VERSION 3.7)
project(polymer-scene-editor LANGUAGES CXX)

file(GLOB INCLUDE_FILES *.hpp)
file(GLOB SOURCE_FILES *.cpp)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

IF(MSVC)
   ADD_DEFINITIONS(/arch:AVX)
ENDIF(MSVC)

set(COMMON_FILES
   "${CMAKE_CURRENT_SOURCE_DIR}/editor-app.hpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/asset-browser.hpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/editor-inspector-ui.hpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/gizmo-controller.hpp" 
   "${CMAKE_CURRENT_SOURCE_DIR}/material-editor.hpp"                               
)
source_group("Common" FILES ${COMMON_FILES}) # note: for this to work as a project file directory/group, needs to be added to the target (e.g. add_executable)

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

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "apps")