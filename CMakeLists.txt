cmake_minimum_required(VERSION 3.20)
project(spurv)

set(THIRDPARTY_DIR ${CMAKE_CURRENT_LIST_DIR}/3rdparty)
set(THIRDPARTY_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/3rdparty)

if (ASAN)
    message("-- Enabling address sanitizer")
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

add_subdirectory(3rdparty)
add_subdirectory(src)
