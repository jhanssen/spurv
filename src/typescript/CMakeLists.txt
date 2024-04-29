set(TS_DEPS
    build.js
    package.json
    src/splitCommand.ts
    src/formatArrayBufferView.ts
    src/formatError.ts
    src/global.d.ts
    src/ProcessStderrEvent.ts
    src/ProcessOptions.ts
    src/Process.ts
    src/concatArrayBuffers.ts
    src/formatArrayBuffer.ts
    src/ProcessStdoutEvent.ts
    src/index.ts
    src/log.ts
    src/test.ts
    src/format.ts
    src/ProcessFinishedEvent.ts
    src/spurv.d.ts
    src/assert.ts
    src/ProcessData.ts
    src/ProcessResult.ts
    tsconfig.json)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dist/spurv.js COMMAND node ${CMAKE_CURRENT_LIST_DIR}/build.js DEPENDS ${TS_DEPS})
add_custom_target(spurv-js DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dist/spurv.js)
