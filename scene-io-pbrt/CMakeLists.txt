cmake_minimum_required(VERSION 3.10.0)

project(scene_io_pbrt VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)

add_library(scene_io_pbrt STATIC
    pbrt_loader.cpp
    pbrt_loader.h
)

target_include_directories(scene_io_pbrt
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../scene-core
)

target_link_libraries(scene_io_pbrt
    PUBLIC
        scene_core
)