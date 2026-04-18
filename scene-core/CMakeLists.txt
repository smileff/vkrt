cmake_minimum_required(VERSION 3.10.0)

project(scene_core VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)

add_library(scene_core STATIC
    scene.cpp
    scene.h
)

target_include_directories(scene_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)