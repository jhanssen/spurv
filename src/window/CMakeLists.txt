set(SOURCES
    Window.cpp
)

if (USE_GLFW)
    list(APPEND SOURCES
        glfw/Window_glfw.cpp
    )
endif()

add_library(spurv-window OBJECT ${SOURCES})
add_library(Spurv::Window ALIAS spurv-window)
target_link_libraries(spurv-window Spurv::Signal)
target_include_directories(spurv-window PRIVATE ${CMAKE_CURRENT_LIST_DIR})
