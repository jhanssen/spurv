set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/finddeps.cmake)

set(SOURCES
    main.cpp
)

set(LIBS
    Fmt::Fmt
    FontConfig::FontConfig
    FunctionTraits::FunctionTraits
    Glfw::Glfw
    Harfbuzz::Harfbuzz
    Rope::Rope
    Vulkan::Vulkan
    msdf-atlas-gen::msdf-atlas-gen
    simdutf::simdutf
    vk-bootstrap::vk-bootstrap
)

link_libraries(${LIBS})
add_compile_options(-Wall -Wextra -pedantic -Werror -Wno-comment)

add_subdirectory(signal)
add_subdirectory(text)

add_executable(spurv ${SOURCES} $<TARGET_OBJECTS:Spurv::Text>)
target_link_libraries(spurv Spurv::Signal)
