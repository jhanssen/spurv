set(SOURCES
    ThreadPool.cpp
)

add_library(spurv-thread OBJECT ${SOURCES})
add_library(Spurv::Thread ALIAS spurv-thread)
target_link_libraries(spurv-thread Spurv::Event)
target_include_directories(spurv-thread PUBLIC ${CMAKE_CURRENT_LIST_DIR})
