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

target_include_directories(quickjs PUBLIC ${THIRDPARTY_DIR}/quickjs)
