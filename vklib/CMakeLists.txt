cmake_minimum_required(VERSION 3.10.0)
project(vklib VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)

# include(CTest)
# enable_testing()

# Add imgui lib.
list(APPEND IMGUI_SOURCES
    $ENV{LIBS_ROOT}/imgui/imgui.cpp
    $ENV{LIBS_ROOT}/imgui/imgui_draw.cpp
    $ENV{LIBS_ROOT}/imgui/imgui_tables.cpp
    $ENV{LIBS_ROOT}/imgui/imgui_widgets.cpp
    $ENV{LIBS_ROOT}/imgui/imgui_demo.cpp
    $ENV{LIBS_ROOT}/imgui/backends/imgui_impl_sdl2.cpp
    $ENV{LIBS_ROOT}/imgui/backends/imgui_impl_vulkan.cpp
    $ENV{LIBS_ROOT}/imgui/misc/cpp/imgui_stdlib.cpp 
    $ENV{LIBS_ROOT}/imgui/misc/debuggers/imgui.natvis
    $ENV{LIBS_ROOT}/imgui/misc/debuggers/imgui.natstepfilter
)
source_group("imgui" FILES ${IMGUI_SOURCES})

list(APPEND SOURCES
    log.h
    vklib.h
    vklib_function.cpp
    vklib_function.h
    vklib_context.cpp
    vklib_context.h
    imgui_function.cpp
    imgui_function.h
    fps_counter.cpp
    fps_counter.h
    sdl_context.cpp
    sdl_context.h
    vma_usage.cpp
    vma_usage.h
    vklib_buffer_alloc.cpp
    vklib_buffer_alloc.h
)
add_library(vklib STATIC ${SOURCES} ${IMGUI_SOURCES})

list(APPEND EXTRA_INCLUDE_PATH
    $ENV{VULKAN_SDK}/Include
    $ENV{VULKAN_SDK}/Include/SDL2
    $ENV{LIBS_ROOT}/imgui
    $ENV{LIBS_ROOT}/imgui/backends
    $ENV{LIBS_ROOT}/eigen
)
target_include_directories(${PROJECT_NAME} PUBLIC ${EXTRA_INCLUDE_PATH})

list(APPEND EXTRA_LIBS
    $ENV{VULKAN_SDK}/Lib/vulkan-1.lib
    $ENV{VULKAN_SDK}/Lib/SDL2.lib
    $ENV{VULKAN_SDK}/Lib/SDL2main.lib
)
target_link_libraries(${PROJECT_NAME} PUBLIC ${EXTRA_LIBS})

# Ends here.