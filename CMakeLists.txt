cmake_minimum_required(VERSION 3.20)
project(spurv)

find_program(CCACHE_FOUND ccache)
if (CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif ()

set(THIRDPARTY_DIR ${CMAKE_CURRENT_LIST_DIR}/3rdparty)
set(THIRDPARTY_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/3rdparty)

# Native node module builds are driven by cmake-js, which passes a toolchain
# file defining these variables. In that mode the project is linked as a .node
# addon instead of a standalone executable.
if (DEFINED NODEJS_CMAKE_DIRECTORY OR DEFINED CMAKE_JS_INC)
    set(SPURV_BUILD_AS_NODE_MODULE ON)
else()
    set(SPURV_BUILD_AS_NODE_MODULE OFF)
endif()

if (ASAN)
    message("-- Enabling address sanitizer")
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

add_subdirectory(3rdparty)
add_subdirectory(src)
