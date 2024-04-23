set(SOURCES
    JSExtra.cpp
    ScriptEngine.cpp
    ScriptValue.cpp
)

add_library(spurv-script OBJECT ${SOURCES})
add_library(Spurv::Script ALIAS spurv-script)
target_link_libraries(spurv-script Spurv::Common fmt::fmt)
target_include_directories(spurv-script PUBLIC ${CMAKE_CURRENT_LIST_DIR})
add_dependencies(spurv-script spurv-js)
