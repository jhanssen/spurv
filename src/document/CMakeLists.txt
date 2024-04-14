set(SOURCES
    Document.cpp
)

add_library(spurv-document OBJECT ${SOURCES})
add_library(Spurv::Document ALIAS spurv-document)
target_link_libraries(spurv-document Spurv::Event Spurv::Thread)
target_include_directories(spurv-document INTERFACE ${CMAKE_CURRENT_LIST_DIR})
