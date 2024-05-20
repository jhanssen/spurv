set(SOURCES
    EventLoop.cpp
    EventLoopUv.cpp
)

if (USE_GLFW)
    list(APPEND SOURCES
        glfw/EventLoop_glfw.cpp
    )
endif()

add_library(spurv-event-object OBJECT ${SOURCES})
add_library(Event::Object ALIAS spurv-event-object)
target_link_libraries(spurv-event-object PRIVATE Common)
target_link_libraries_system(spurv-event-object PRIVATE libuv::libuv glfw::glfw)
target_include_directories(spurv-event-object PRIVATE ${CMAKE_CURRENT_LIST_DIR})

add_library(spurv-event-interface INTERFACE)
add_library(Event ALIAS spurv-event-interface)
target_include_directories(spurv-event-interface INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(spurv-event-interface INTERFACE Window)
