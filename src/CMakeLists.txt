set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/finddeps.cmake)

set(SOURCES
    main.cpp
)

set(LIBS
    FontConfig::FontConfig
    Fmt::Fmt
    Glfw::Glfw
    Harfbuzz::Harfbuzz
    Rope::Rope
    Vulkan::Vulkan
    msdf-atlas-gen::msdf-atlas-gen
    simdutf::simdutf
    vk-bootstrap::vk-bootstrap
)

link_libraries(${LIBS})
add_compile_options(-Wall -Wextra -pedantic -Werror)

add_subdirectory(text)

add_executable(spurv ${SOURCES} $<TARGET_OBJECTS:spurv-text>)