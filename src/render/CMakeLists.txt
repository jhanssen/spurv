set(SOURCES
    GenericPool.cpp
    GlyphAtlas.cpp
    SemaphorePool.cpp
    Renderer.cpp
    VmaImplementation.cpp
)

add_library(spurv-render OBJECT ${SOURCES})
add_library(Spurv::Render ALIAS spurv-render)
target_link_libraries(spurv-render Spurv::Event Spurv::Thread Spurv::Window msdf-atlas-gen::msdf-atlas-gen GPUOpen::VulkanMemoryAllocator)
target_include_directories(spurv-render INTERFACE ${CMAKE_CURRENT_LIST_DIR})
