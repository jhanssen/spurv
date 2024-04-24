set(TS_DEPS
    build.js
    package.json
    src/Process.ts
    src/ProcessData.ts
    src/ProcessFinishedEvent.ts
    src/ProcessOptions.ts
    src/ProcessResult.ts
    src/ProcessStderrEvent.ts
    src/ProcessStdoutEvent.ts
    src/assert.ts
    src/concatArrayBuffers.ts
    src/format.ts
    src/index.ts
    src/log.ts
    src/splitCommand.ts
    src/spurv.d.ts
    tsconfig.json)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dist/spurv.js COMMAND node ${CMAKE_CURRENT_LIST_DIR}/build.js DEPENDS ${TS_DEPS})
add_custom_target(spurv-js DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dist/spurv.js)
