set(SOURCES
    AppPath.cpp
    Color.cpp
    FS.cpp
    Thread.cpp
)

add_library(spurv-common OBJECT ${SOURCES})
add_library(Spurv::Common ALIAS spurv-common)
target_include_directories(spurv-common INTERFACE ${CMAKE_CURRENT_LIST_DIR})
