set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/finddeps.cmake)

set(SOURCES
    main.cpp
)

set(LIBS
    FontConfig::FontConfig
    Fmt::Fmt
)

link_libraries(${LIBS})

add_subdirectory(text)

add_executable(spurv ${SOURCES} $<TARGET_OBJECTS:spurv-text>)
