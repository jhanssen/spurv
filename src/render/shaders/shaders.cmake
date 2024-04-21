set(GLSLC ${THIRDPARTY_BINARY_DIR}/shaderc/glslc/glslc)
set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_BINARY_DIR ${CMAKE_BINARY_DIR}/bin/shaders)
file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})
add_dependencies(spurv-render glslc_exe)

macro(add_shader FILE STAGE)
    add_custom_command(
        OUTPUT ${SHADER_BINARY_DIR}/${FILE}.spv
        COMMAND ${GLSLC} -fshader-stage=${STAGE} ${SHADER_SOURCE_DIR}/${FILE}.glsl -o ${SHADER_BINARY_DIR}/${FILE}.spv
        DEPENDS glslc_exe ${SHADER_SOURCE_DIR}/${FILE}.glsl)
    add_custom_target(${FILE} DEPENDS ${SHADER_BINARY_DIR}/${FILE}.spv)
    add_dependencies(spurv-render ${FILE})
endmacro()

add_shader(text-vs vertex)
add_shader(text-fs fragment)
