cmake_minimum_required(VERSION 3.10.0)
project(VKMeshViewer VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)

list(APPEND SOURCES
    VKMeshViewerMain.cpp
    OBJMesh.cpp
    OBJMesh.h
    scene.cpp
    scene.h
)

list(APPEND SHADER_SOURCES
    shaders/test.vert
    shaders/test.frag
)
source_group("shaders" FILES ${SHADER_SOURCES})

add_executable(${PROJECT_NAME} ${SOURCES} ${SHADER_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE "../")
target_link_libraries(${PROJECT_NAME} PRIVATE vklib)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>")

# Compile shaders.
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CompileGLSL.cmake)

foreach(SHADER_FILE ${SHADER_SOURCES})
    get_filename_component(FILE_EXT ${SHADER_FILE} EXT)
    if(NOT ${FILE_EXT} STREQUAL ".h")        
        CompileGLSL(${SHADER_FILE} "")
    endif()
endforeach()