cmake_policy(SET CMP0077 NEW)

set(FMT_INSTALL OFF CACHE BOOL "")

set(FT_DISABLE_ZLIB ON CACHE BOOL "")
set(FT_DISABLE_PNG ON CACHE BOOL "")
set(FT_DISABLE_BZIP2 ON CACHE BOOL "")
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "")
set(FT_DISABLE_BROTLI ON CACHE BOOL "")

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
set(GLFW_INSTALL OFF CACHE BOOL "")

set(LIBUV_BUILD_SHARED OFF CACHE BOOL "")

set(MSDF_ATLAS_USE_VCPKG OFF CACHE BOOL "")
set(MSDF_ATLAS_USE_SKIA OFF CACHE BOOL "")
set(MSDFGEN_DISABLE_PNG ON)

set(SKIP_SPIRV_TOOLS_INSTALL ON)

set(SHADERC_SKIP_INSTALL ON)
set(SHADERC_SKIP_TESTS ON)
set(SHADERC_SKIP_EXAMPLES ON)
set(SHADERC_SKIP_COPYRIGHT_CHECK ON)

set(SIMDUTF_TESTS OFF CACHE BOOL "")
set(SIMDUTF_BENCHMARKS OFF CACHE BOOL "")
set(SIMDUTF_TOOLS OFF CACHE BOOL "")
set(SIMDUTF_CXX_STANDARD 20 CACHE STRING "")

set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "")

set(VULKAN_HEADERS_INSTALL_DIR ${CMAKE_CURRENT_LIST_DIR}/Vulkan-Headers CACHE PATH "")

set(VK_BOOTSTRAP_VULKAN_HEADER_DIR ${CMAKE_CURRENT_LIST_DIR}/Vulkan-Headers/include CACHE FILEPATH "")

add_subdirectory(FunctionTraits)
add_subdirectory(VulkanMemoryAllocator)
add_subdirectory(fmt)
add_subdirectory(freetype)
add_subdirectory(glfw)
add_subdirectory(harfbuzz)
add_subdirectory(libqss)
add_subdirectory(libuv)

# hack to make msdfgen pick up freetype
add_library(Freetype::Freetype ALIAS freetype-interface)

add_subdirectory(msdf-atlas-gen)

# these have to be before shaderc
add_subdirectory(SPIRV-Headers)
add_subdirectory(SPIRV-Tools)
add_subdirectory(glslang)

add_subdirectory(shaderc)
add_subdirectory(simdutf)
add_subdirectory(spdlog)
add_subdirectory(vk-bootstrap)
add_subdirectory(volk)

# hack to make freetype pick up harfbuzz
target_link_libraries(freetype PRIVATE harfbuzz)

include(quickjs.cmake)
#if (APPLE)
#    include(MoltenVK.cmake)
#endif()

add_library(glfw::glfw ALIAS glfw)
add_library(harfbuzz::harfbuzz ALIAS harfbuzz)

# remove fmt as an interface library for spdlog, we don't want it appearing on our link line twice
get_target_property(SPDLOG_IFACE_LINKED_LIBS spdlog INTERFACE_LINK_LIBRARIES)
list(REMOVE_ITEM SPDLOG_IFACE_LINKED_LIBS fmt::fmt)
set_property(TARGET spdlog PROPERTY INTERFACE_LINK_LIBRARIES ${SPDLOG_IFACE_LINKED_LIBS})
