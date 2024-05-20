set(SOURCES
    Document.cpp
    Layout.cpp
    Styleable.cpp
    TextClasses.cpp
    rope/rope.cpp
    rope/node.cpp
)

add_library(spurv-document-object OBJECT ${SOURCES})
add_library(Document::Object ALIAS spurv-document-object)
target_link_libraries(spurv-document-object PRIVATE Event Text Thread Common)
target_link_libraries_system(spurv-document-object PRIVATE qss::qss yogacore::yogacore uni-algo::uni-algo simdutf::simdutf)
target_include_directories(spurv-document-object PRIVATE ${CMAKE_CURRENT_LIST_DIR}/rope)

add_library(spurv-document-interface INTERFACE)
add_library(Document ALIAS spurv-document-interface)
target_include_directories(spurv-document-interface INTERFACE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/rope)
target_link_libraries(spurv-document-interface INTERFACE Common)
target_link_libraries_system(spurv-document-interface INTERFACE qss::qss yogacore::yogacore)
