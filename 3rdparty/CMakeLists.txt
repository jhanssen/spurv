set(MSDF_ATLAS_USE_VCPKG OFF)
set(MSDF_ATLAS_USE_SKIA OFF)

set(SIMDUTF_TESTS OFF)
set(SIMDUTF_BENCHMARKS OFF)
set(SIMDUTF_TOOLS OFF)
set(SIMDUTF_CXX_STANDARD 20 CACHE STRING "")

set(VK_BOOTSTRAP_VULKAN_HEADER_DIR ${CMAKE_CURRENT_LIST_DIR}/Vulkan-Headers/include)

add_subdirectory(msdf-atlas-gen)
add_subdirectory(rope)
add_subdirectory(simdutf)
add_subdirectory(vk-bootstrap)
