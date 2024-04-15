cmake_policy(SET CMP0077 NEW)

set(FMT_INSTALL OFF CACHE BOOL "")

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
set(GLFW_INSTALL OFF CACHE BOOL "")

set(LIBUV_BUILD_SHARED OFF CACHE BOOL "")

set(MSDF_ATLAS_USE_VCPKG OFF CACHE BOOL "")
set(MSDF_ATLAS_USE_SKIA OFF CACHE BOOL "")

set(SIMDUTF_TESTS OFF CACHE BOOL "")
set(SIMDUTF_BENCHMARKS OFF CACHE BOOL "")
set(SIMDUTF_TOOLS OFF CACHE BOOL "")
set(SIMDUTF_CXX_STANDARD 20 CACHE STRING "")

set(VK_BOOTSTRAP_VULKAN_HEADER_DIR ${CMAKE_CURRENT_LIST_DIR}/Vulkan-Headers/include CACHE FILEPATH "")

add_subdirectory(fmt)
add_subdirectory(glfw)
add_subdirectory(harfbuzz)
add_subdirectory(libuv)
add_subdirectory(msdf-atlas-gen)
add_subdirectory(simdutf)
add_subdirectory(vk-bootstrap)
add_subdirectory(libqss)
add_subdirectory(FunctionTraits)
add_subdirectory(VulkanMemoryAllocator)

add_library(glfw::glfw ALIAS glfw)
add_library(harfbuzz::harfbuzz ALIAS harfbuzz)
