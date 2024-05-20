set(SOURCES
    ThreadPool.cpp
)

add_library(spurv-thread-object OBJECT ${SOURCES})
add_library(Thread::Object ALIAS spurv-thread-object)
target_link_libraries(spurv-thread-object PRIVATE Event Common)
target_link_libraries_system(spurv-thread-object PRIVATE FunctionTraits::FunctionTraits)
target_include_directories(spurv-thread-object PRIVATE ${CMAKE_CURRENT_LIST_DIR})

add_library(spurv-thread-interface INTERFACE)
add_library(Thread ALIAS spurv-thread-interface)
target_include_directories(spurv-thread-interface INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries_system(spurv-thread-interface INTERFACE FunctionTraits::FunctionTraits)
