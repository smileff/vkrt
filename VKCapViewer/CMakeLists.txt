cmake_minimum_required(VERSION 3.10.0)
project(VKCapViewer VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)

# include(CTest)
# enable_testing()

# Project settings.

list(APPEND SOURCES
    VKCapViewerMain.cpp
)
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE "../")

target_link_libraries(${PROJECT_NAME} PRIVATE vklib)

# Extra include 
list(APPEND EXTRA_INCLUDE_PATH
)
target_include_directories(${PROJECT_NAME} PUBLIC ${EXTRA_INCLUDE_PATH})

# Extra lib
list(APPEND EXTRA_LIBS
)
target_link_libraries(${PROJECT_NAME} PUBLIC ${EXTRA_LIBS})

# Ends here.