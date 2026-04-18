cmake_minimum_required(VERSION 3.10.0)

project(cpu_rt VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)

add_library(cpu_rt STATIC
    cpu_rt.cpp
    cpu_rt.h
)

target_include_directories(cpu_rt
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../scene-core
)