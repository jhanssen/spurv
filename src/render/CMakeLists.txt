set(SOURCES
    GenericPool.cpp
    SemaphorePool.cpp
    Renderer.cpp
)

add_library(spurv-render OBJECT ${SOURCES})
add_library(Spurv::Render ALIAS spurv-render)
target_link_libraries(spurv-render Spurv::Event Spurv::Window)
target_include_directories(spurv-render INTERFACE ${CMAKE_CURRENT_LIST_DIR})
