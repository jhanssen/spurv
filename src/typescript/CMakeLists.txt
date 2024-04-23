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
    src/log.ts
    src/splitCommand.ts
    src/spurv.ts
    src/types.d.ts
    tsconfig.json)

add_custom_target(spurv_js ALL COMMAND node ${CMAKE_CURRENT_LIST_DIR}/build.js)

