add_library(base64 INTERFACE
    ${THIRDPARTY_DIR}/base64/include/base64.hpp
)

add_library(base64::base64 ALIAS base64)

target_include_directories(quickjs INTERFACE ${THIRDPARTY_DIR}/base64/include/)
