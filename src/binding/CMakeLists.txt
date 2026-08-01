# The .d.ts files the binding generator parses (the API contract). Symbols
# annotated with @native are bound; everything else is ignored for now.
set(SPURV_BINDING_DTS
    ${CMAKE_SOURCE_DIR}/src/typescript/src/spurv.d.ts
)

set(SPURV_BINDING_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(SPURV_BINDING_GENERATED
    ${SPURV_BINDING_GENERATED_DIR}/bindings.cpp
    ${SPURV_BINDING_GENERATED_DIR}/bindings.h
)

# Requires `npm install` (typescript@6) at the repo root: the generator does
# require("typescript"), which resolves up the tree from the build dir.
add_custom_command(
    OUTPUT ${SPURV_BINDING_GENERATED}
    COMMAND node ${CMAKE_CURRENT_SOURCE_DIR}/generate.js
            --dts ${SPURV_BINDING_DTS}
            --out ${SPURV_BINDING_GENERATED_DIR}
    DEPENDS generate.js ${SPURV_BINDING_DTS}
    COMMENT "generating node bindings from .d.ts"
    VERBATIM
)
add_custom_target(spurv-bindings DEPENDS ${SPURV_BINDING_GENERATED})

set(SOURCES
    binding.cpp
    BindingSupport.cpp
    ${SPURV_BINDING_GENERATED_DIR}/bindings.cpp
)

add_library(spurv-binding-object OBJECT ${SOURCES})
add_library(Binding::Object ALIAS spurv-binding-object)
add_dependencies(spurv-binding-object spurv-bindings)
target_link_libraries(spurv-binding-object PRIVATE App Editor Document Event Common Render Text Window Thread)
target_link_libraries_system(spurv-binding-object PRIVATE spdlog::spdlog)
target_include_directories(spurv-binding-object PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${SPURV_BINDING_GENERATED_DIR})
target_include_directories(spurv-binding-object SYSTEM PRIVATE ${CMAKE_JS_INC})
target_compile_definitions(spurv-binding-object PRIVATE NAPI_VERSION=8)
