set(SOURCES
    Container.cpp
    Editor.cpp
    View.cpp
)

add_library(spurv-editor OBJECT ${SOURCES})
add_library(Spurv::Editor ALIAS spurv-editor)
target_link_libraries(spurv-editor Spurv::Event Spurv::Document Spurv::Render Spurv::Script)
target_include_directories(spurv-editor INTERFACE ${CMAKE_CURRENT_LIST_DIR})
