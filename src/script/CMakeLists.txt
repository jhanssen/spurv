set(SOURCES
    Builtins.cpp
    ScriptEngine.cpp
    ScriptEngine_process.cpp
    ScriptValue.cpp
    ScriptClass.cpp
)

add_library(spurv-script OBJECT ${SOURCES})
add_library(Spurv::Script ALIAS spurv-script)
target_link_libraries(spurv-script Spurv::Common fmt::fmt Spurv::Event base64::base64 Spurv::Thread)
target_include_directories(spurv-script PUBLIC ${CMAKE_CURRENT_LIST_DIR})
add_dependencies(spurv-script spurv-js)
