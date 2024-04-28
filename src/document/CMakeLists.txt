set(SOURCES
    Document.cpp
    Layout.cpp
    Styleable.cpp
    rope/rope.cpp
    rope/node.cpp
)

add_library(spurv-document OBJECT ${SOURCES})
add_library(Spurv::Document ALIAS spurv-document)
target_link_libraries(spurv-document Spurv::Event Spurv::Text Spurv::Thread Spurv::Common)
target_include_directories(spurv-document
    INTERFACE ${CMAKE_CURRENT_LIST_DIR}
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/rope
)
