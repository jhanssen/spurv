set(SOURCES
    AppPath.cpp
    Color.cpp
    FS.cpp
    Thread.cpp
)

add_library(spurv-common-object OBJECT ${SOURCES})
add_library(Common::Object ALIAS spurv-common-object)
target_link_libraries_system(spurv-common-object PRIVATE unordered_dense::unordered_dense FunctionTraits::FunctionTraits fmt::fmt spdlog::spdlog volk::volk)

add_library(spurv-common-interface INTERFACE)
add_library(Common ALIAS spurv-common-interface)
target_include_directories(spurv-common-interface INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries_system(spurv-common-interface INTERFACE unordered_dense::unordered_dense FunctionTraits::FunctionTraits fmt::fmt spdlog::spdlog volk::volk)

