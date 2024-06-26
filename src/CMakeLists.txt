set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/target_link_libraries_system.cmake)
include(cmake/finddeps.cmake)

set(SOURCES
    main.cpp
)

set(LIBS
    FontConfig::FontConfig
    FunctionTraits::FunctionTraits
    GPUOpen::VulkanMemoryAllocator
    volk::volk
    fmt::fmt
    glfw::glfw
    harfbuzz::harfbuzz
    libuv::libuv
    msdf-atlas-gen::msdf-atlas-gen
    simdutf::simdutf
    spdlog::spdlog
    uni-algo::uni-algo
    unordered_dense::unordered_dense
    vk-bootstrap::vk-bootstrap
    qss::qss
    quickjs::quickjs
    yogacore::yogacore
)

# # add all LIBS interface dirs as include dirs to all of spurv
# foreach(LIB ${LIBS})
#     get_target_property(IFACE ${LIB} INTERFACE_INCLUDE_DIRECTORIES)
#     if (IFACE)
#         include_directories(SYSTEM ${IFACE})
#     endif()
# endforeach()

add_compile_options(-Wall -Wextra -pedantic -Werror -Wno-comment)

set(USE_GLFW ON)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/config ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(app)
add_subdirectory(common)
add_subdirectory(document)
add_subdirectory(editor)
add_subdirectory(event)
add_subdirectory(render)
add_subdirectory(script)
add_subdirectory(text)
add_subdirectory(thread)
add_subdirectory(typescript)
add_subdirectory(window)

add_executable(spurv ${SOURCES})
target_link_libraries(spurv
    App
    Common::Object
    Document::Object
    Editor::Object
    Event::Object
    Render::Object
    Script::Object
    Text::Object
    Thread::Object
    Window::Object
    ${LIBS})

set_target_properties(spurv PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
