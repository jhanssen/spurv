set(FunctionTraits_INCLUDES ${CMAKE_CURRENT_LIST_DIR})
add_library(FunctionTraits INTERFACE)
add_library(FunctionTraits::FunctionTraits ALIAS FunctionTraits)
target_include_directories(FunctionTraits INTERFACE ${FunctionTraits_INCLUDES})
