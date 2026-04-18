cmake_minimum_required(VERSION 3.10.0)

set(CMAKE_CXX_STANDARD 20)

project(vkrt)

# Register API-independent core modules first.
add_subdirectory(scene-core)
add_subdirectory(scene-io-pbrt)
add_subdirectory(cpu-rt)

# Register rendering backends and application entry points.
add_subdirectory(vklib)
add_subdirectory(vkrt)
# add_subdirectory(VKInitTest)

# Add documentation files to the IDE view.
set(PROJECT_DOCS_DIR "${CMAKE_SOURCE_DIR}/docs")
if(EXISTS "${PROJECT_DOCS_DIR}")
    file(GLOB_RECURSE PROJECT_DOC_FILES CONFIGURE_DEPENDS
        "${PROJECT_DOCS_DIR}/*.md"
        "${PROJECT_DOCS_DIR}/*.txt"
        "${PROJECT_DOCS_DIR}/*.rst"
    )
    add_custom_target(docs)
    set_property(TARGET docs PROPERTY SOURCES ${PROJECT_DOC_FILES})
endif()

# Mirror the repository .github directory into the build tree with a symlink only.
set(COPILOT_GITHUB_SRC "${CMAKE_SOURCE_DIR}/.github")
set(COPILOT_GITHUB_DST "${CMAKE_BINARY_DIR}/.github")

# Skip setup when the repository .github directory does not exist.
if(EXISTS "${COPILOT_GITHUB_SRC}")
    get_filename_component(COPILOT_GITHUB_SRC_ABS "${COPILOT_GITHUB_SRC}" ABSOLUTE)

    # Accept an existing symlink only when it already points to the expected source directory.
    if(IS_SYMLINK "${COPILOT_GITHUB_DST}")
        file(READ_SYMLINK "${COPILOT_GITHUB_DST}" COPILOT_GITHUB_DST_TARGET)
        get_filename_component(
            COPILOT_GITHUB_DST_TARGET_ABS
            "${COPILOT_GITHUB_DST_TARGET}"
            ABSOLUTE
            BASE_DIR "${CMAKE_BINARY_DIR}"
        )

        if(NOT COPILOT_GITHUB_DST_TARGET_ABS STREQUAL COPILOT_GITHUB_SRC_ABS)
            message(FATAL_ERROR "${COPILOT_GITHUB_DST} already exists as a symlink to ${COPILOT_GITHUB_DST_TARGET_ABS}, expected ${COPILOT_GITHUB_SRC_ABS}.")
        endif()
    # Reject an existing real directory or file with the same name.
    elseif(EXISTS "${COPILOT_GITHUB_DST}")
        message(FATAL_ERROR "${COPILOT_GITHUB_DST} already exists and is not a symbolic link.")
    else()
        # Create the missing symlink and fail fast if the platform cannot create it.
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E create_symlink "${COPILOT_GITHUB_SRC}" "${COPILOT_GITHUB_DST}"
            RESULT_VARIABLE _github_link_result
            OUTPUT_QUIET
            ERROR_QUIET
        )

        if(NOT _github_link_result EQUAL 0)
            message(FATAL_ERROR "Failed to create symlink from ${COPILOT_GITHUB_SRC} to ${COPILOT_GITHUB_DST}.")
        endif()
    endif()
endif()