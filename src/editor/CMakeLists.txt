set(SOURCES
    Container.cpp
    Cursor.cpp
    Editor.cpp
    Frame.cpp
    View.cpp
)

add_library(spurv-editor-object OBJECT ${SOURCES})
add_library(Editor::Object ALIAS spurv-editor-object)
target_link_libraries(spurv-editor-object PRIVATE Event Document Render Script Window)

add_library(spurv-editor-interface INTERFACE)
add_library(Editor ALIAS spurv-editor-interface)
target_include_directories(spurv-editor-interface INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(spurv-editor-interface INTERFACE Render Document)
