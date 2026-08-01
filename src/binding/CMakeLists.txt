set(SOURCES
    binding.cpp
)

add_library(spurv-binding-object OBJECT ${SOURCES})
add_library(Binding::Object ALIAS spurv-binding-object)
target_link_libraries(spurv-binding-object PRIVATE App)
target_include_directories(spurv-binding-object PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(spurv-binding-object SYSTEM PRIVATE ${CMAKE_JS_INC})
target_compile_definitions(spurv-binding-object PRIVATE NAPI_VERSION=8)
