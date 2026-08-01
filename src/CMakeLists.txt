set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/target_link_libraries_system.cmake)
include(cmake/finddeps.cmake)

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
add_subdirectory(text)
add_subdirectory(thread)
add_subdirectory(typescript)
add_subdirectory(window)

if (SPURV_BUILD_AS_NODE_MODULE)
    add_subdirectory(binding)
endif()

# When cmake-js is driving the configure it defines the variables detected in
# the top-level CMakeLists.txt (SPURV_BUILD_AS_NODE_MODULE). In that mode we
# build a native node addon (spurv.node) instead of the standalone executable,
# so main.cpp is excluded and the addon entry point (src/binding) is used.
if (SPURV_BUILD_AS_NODE_MODULE)
    set(SOURCES
        ${CMAKE_JS_SRC}
    )
else()
    set(SOURCES
        main.cpp
    )
endif()

set(SPURV_LIBRARIES
    App
    Common::Object
    Document::Object
    Editor::Object
    Event::Object
    Render::Object
    Text::Object
    Thread::Object
    Window::Object
    ${LIBS})

if (SPURV_BUILD_AS_NODE_MODULE)
    add_library(spurv SHARED ${SOURCES})
    target_link_libraries(spurv PRIVATE Binding::Object ${CMAKE_JS_LIB})
    set_target_properties(spurv PROPERTIES
        PREFIX ""
        SUFFIX ".node"
    )
    if (MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
        execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
    endif ()
else()
    add_executable(spurv ${SOURCES})
    set_target_properties(spurv PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif ()

target_link_libraries(spurv PRIVATE ${SPURV_LIBRARIES})

# the typescript bundle is built under cmake control in both modes
add_dependencies(spurv spurv-js)
