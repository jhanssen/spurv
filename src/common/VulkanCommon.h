#pragma once

#include "Logger.h"
#include <cstdlib>

#define VK_CHECK_SUCCESS(function)                                      \
    do { VkResult result = function;                                    \
        if(result != VK_SUCCESS) {                                      \
            spdlog::error("Vulkan failure: {} [{}]", #function, result); \
            abort();                                                    \
        } } while (0)
