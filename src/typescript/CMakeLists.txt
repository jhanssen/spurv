set(TS_DEPS
    build.js
    package.json
    tsconfig.json
    src/format.ts
    src/spurv.ts
    src/log.ts)

add_custom_target(spurv_js ALL COMMAND node ${CMAKE_CURRENT_LIST_DIR}/build.js)

