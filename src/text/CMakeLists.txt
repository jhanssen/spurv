set(SOURCES
    Font.cpp
)

add_library(spurv-text OBJECT ${SOURCES})
add_library(Spurv::Text ALIAS spurv-text)
target_link_libraries(spurv-text Spurv::Event)
target_include_directories(spurv-text PUBLIC ${CMAKE_CURRENT_LIST_DIR})
