add_library(quickjs OBJECT
    ${THIRDPARTY_DIR}/quickjs/quickjs.h
    ${THIRDPARTY_DIR}/quickjs/quickjs-libc.h
    ${THIRDPARTY_DIR}/quickjs/quickjs.c
    ${THIRDPARTY_DIR}/quickjs/libregexp.c
    ${THIRDPARTY_DIR}/quickjs/libunicode.c
    ${THIRDPARTY_DIR}/quickjs/libbf.c
    ${THIRDPARTY_DIR}/quickjs/cutils.c
    ${THIRDPARTY_DIR}/quickjs/quickjs-libc.c
)

add_library(quickjs::quickjs ALIAS quickjs)

set(QUICKJS_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/quickjs-include)
file(MAKE_DIRECTORY ${QUICKJS_INCLUDE})
file(COPY ${THIRDPARTY_DIR}/quickjs/quickjs.h DESTINATION ${QUICKJS_INCLUDE})

target_include_directories(quickjs PRIVATE ${THIRDPARTY_DIR}/quickjs)
target_include_directories(quickjs INTERFACE ${QUICKJS_INCLUDE})
target_compile_options(quickjs PRIVATE -DCONFIG_VERSION=\"2024-02-14\" -DCONFIG_BIGNUM)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(quickjs PRIVATE -D_GNU_SOURCE)
endif ()
