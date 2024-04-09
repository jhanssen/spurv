cmake_minimum_required(VERSION 3.20)

project(ProjRope
    LANGUAGES CXX
    DESCRIPTION "Rope impl from https://github.com/wroever/rope"
    VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ProjRope STATIC node.cpp node.hpp rope.cpp rope.hpp)
add_library(Rope::Rope ALIAS ProjRope)
target_include_directories(ProjRope PUBLIC ${CMAKE_CURRENT_LIST_DIR})
