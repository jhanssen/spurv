set(SOURCES
    EventLoop.cpp
)

if (USE_GLFW)
    list(APPEND SOURCES
        glfw/EventLoop_glfw.cpp
    )
endif()

add_library(spurv-event OBJECT ${SOURCES})
add_library(Spurv::Event ALIAS spurv-event)
target_link_libraries(spurv-event)
target_include_directories(spurv-event PUBLIC ${CMAKE_CURRENT_LIST_DIR})
