set(SOURCES
    Window.cpp
)

if (USE_GLFW)
    list(APPEND SOURCES
        glfw/Window_glfw.cpp
    )
endif()

add_library(spurv-window-object OBJECT ${SOURCES})
add_library(Window::Object ALIAS spurv-window-object)
target_link_libraries(spurv-window-object PRIVATE Event Common)
target_link_libraries_system(spurv-window-object PRIVATE volk::volk glfw::glfw)
target_include_directories(spurv-window-object PRIVATE ${CMAKE_CURRENT_LIST_DIR})

add_library(spurv-window-interface INTERFACE)
add_library(Window ALIAS spurv-window-interface)
target_include_directories(spurv-window-interface INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(spurv-window-interface INTERFACE Common)
target_link_libraries_system(spurv-window-interface INTERFACE glfw::glfw)
