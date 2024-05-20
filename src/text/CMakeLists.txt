set(SOURCES
    Font.cpp
)

add_library(spurv-text-object OBJECT ${SOURCES})
add_library(Text::Object ALIAS spurv-text-object)
target_link_libraries(spurv-text-object PRIVATE Event)
target_link_libraries_system(spurv-text-object PRIVATE harfbuzz::harfbuzz)
target_include_directories(spurv-text-object PRIVATE ${CMAKE_CURRENT_LIST_DIR})

add_library(spurv-text-interface INTERFACE)
add_library(Text ALIAS spurv-text-interface)
target_include_directories(spurv-text-interface INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries_system(spurv-text-interface INTERFACE harfbuzz::harfbuzz)
