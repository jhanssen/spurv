set(SOURCES
    Builtins.cpp
    ScriptEngine.cpp
    ScriptEngine_process.cpp
    ScriptEngine_fetch.cpp
    ScriptValue.cpp
    ScriptClass.cpp
)

add_library(spurv-script-object OBJECT ${SOURCES})
add_library(Script::Object ALIAS spurv-script-object)
target_link_libraries(spurv-script-object PRIVATE Common Thread Editor Event)
target_link_libraries_system(spurv-script-object PRIVATE fmt::fmt base64::base64 curl::curl quickjs::quickjs libuv::libuv simdutf::simdutf)
target_include_directories(spurv-script-object PRIVATE ${CMAKE_CURRENT_LIST_DIR})
add_dependencies(spurv-script-object spurv-js)

add_library(spurv-script-interface INTERFACE)
add_library(Script ALIAS spurv-script-interface)
target_include_directories(spurv-script-interface INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries_system(spurv-script-interface INTERFACE quickjs::quickjs libuv::libuv)
