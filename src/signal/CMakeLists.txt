add_library(spurv-signal INTERFACE)
add_library(Spurv::Signal ALIAS spurv-signal)
target_include_directories(spurv-signal INTERFACE ${CMAKE_CURRENT_LIST_DIR})
