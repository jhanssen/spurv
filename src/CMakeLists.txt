set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/finddeps.cmake)

set(SOURCES
    main.cpp
)

set(LIBS
    FontConfig::FontConfig
    FunctionTraits::FunctionTraits
    GPUOpen::VulkanMemoryAllocator
    Vulkan::Vulkan
    fmt::fmt
    glfw::glfw
    harfbuzz::harfbuzz
    libuv::libuv
    msdf-atlas-gen::msdf-atlas-gen
    simdutf::simdutf
    vk-bootstrap::vk-bootstrap
    qss::qss
)

# add all LIBS interface dirs as include dirs to all of spurv
foreach(LIB ${LIBS})
    get_target_property(IFACE ${LIB} INTERFACE_INCLUDE_DIRECTORIES)
    if (IFACE)
        include_directories(${IFACE})
    endif()
endforeach()

add_compile_options(-Wall -Wextra -pedantic -Werror -Wno-comment)

set(USE_GLFW ON)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/config ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/common)

add_subdirectory(window)
add_subdirectory(event)
add_subdirectory(text)
add_subdirectory(thread)
add_subdirectory(document)
add_subdirectory(editor)
add_subdirectory(render)

add_executable(spurv ${SOURCES}
    $<TARGET_OBJECTS:Spurv::Window> $<TARGET_OBJECTS:Spurv::Event> $<TARGET_OBJECTS:Spurv::Text>
    $<TARGET_OBJECTS:Spurv::Thread> $<TARGET_OBJECTS:Spurv::Document> $<TARGET_OBJECTS:Spurv::Editor>
    $<TARGET_OBJECTS:Spurv::Render>
)
target_link_libraries(spurv Spurv::Event ${LIBS})

set_target_properties(spurv PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
