
# Define the path to the GLSL compiler.
if(WIN32)
    set(GLSLCompiler "$ENV{VULKAN_SDK}/Bin/glslc.exe")
elseif(UNIX)
    set(GLSLCompiler "glslangValidator")    
endif()

# CMake function to compile glsl shader to spv.
macro(CompileGLSL SOURCE FLAGS)
    if(GLSLCompiler)            
        set(SHADER_FILEPATH "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE}")
        set(OUTPUT "${PROJECT_BINARY_DIR}/$<CONFIG>/${SOURCE}.spv")        

        if(WIN32) 
          set(_COMMAND ${GLSLCompiler} -o ${OUTPUT} $<$<CONFIG:Debug>:-g> ${FLAGS} ${SHADER_FILEPATH})
        elseif(UNIX)
          set(_COMMAND ${GLSLCompiler} -V -o ${OUTPUT} ${FLAGS} ${SHADER_FILEPATH})
        endif()

        MESSAGE(STATUS "Add command: ${_COMMAND}")

        add_custom_command(
            OUTPUT ${OUTPUT}
            COMMAND echo ${_COMMAND}
            COMMAND ${_COMMAND}
            DEPENDS ${SOURCE}
            MAIN_DEPENDENCY ${SOURCE}
            )
    else(GLSLCompiler)
        MESSAGE(WARNING "could not find GLSLCompiler to compile shaders")
    endif()
endmacro()