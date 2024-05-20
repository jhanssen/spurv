set(SOURCES App.cpp)

add_library(spurv-app OBJECT ${SOURCES})
add_library(App ALIAS spurv-app)
target_link_libraries(spurv-app PRIVATE Common Window Editor Render Event)
target_link_libraries_system(spurv-app PRIVATE libuv::libuv)
target_include_directories(spurv-app PUBLIC ${CMAKE_CURRENT_LIST_DIR})

