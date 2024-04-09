set(SOURCES
    Font.cpp
)

add_library(spurv-text OBJECT ${SOURCES})
add_library(Spurv::Text ALIAS spurv-text)
