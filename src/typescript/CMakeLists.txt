set(TS_DEPS
    src/format.ts
    src/spurv.ts
    src/log.ts)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/spurv.js
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/
    DEPENDS ${TS_DEPS}
    COMMENT "Building typescript"
    COMMAND npm run build -- --outFile ${CMAKE_CURRENT_BINARY_DIR}/spurv.js --module amd)
