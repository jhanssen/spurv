set(SOURCES
    Easing.cpp
    GenericPool.cpp
    GlyphAtlas.cpp
    GraphicsPipeline.cpp
    SemaphorePool.cpp
    Renderer.cpp
    TextVBO.cpp
    VmaImplementation.cpp
)

add_library(spurv-render-object OBJECT ${SOURCES})
add_library(Render::Object ALIAS spurv-render-object)
target_link_libraries(spurv-render-object PRIVATE Event Common Text Thread Window)
target_link_libraries_system(spurv-render-object PRIVATE msdf-atlas-gen::msdf-atlas-gen GPUOpen::VulkanMemoryAllocator volk::volk libuv::libuv vk-bootstrap::vk-bootstrap)

add_library(spurv-render-interface INTERFACE)
add_library(Render ALIAS spurv-render-interface)
target_include_directories(spurv-render-interface INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(spurv-render-interface INTERFACE Text)
target_link_libraries_system(spurv-render-interface INTERFACE volk::volk)

include(shaders/shaders.cmake)
