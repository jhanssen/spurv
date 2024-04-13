#pragma once

#include <fmt/core.h>
#include <cstdlib>

#define VK_CHECK_SUCCESS(function)                                      \
    do { VkResult result = function;                                    \
        if(result != VK_SUCCESS) {                                      \
            fmt::print(stderr, "Vulkan failure: {} [{}]\n", #function, result); \
            abort();                                                    \
        } } while (0)
