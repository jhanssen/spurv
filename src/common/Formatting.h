#pragma once

#include <fmt/core.h>
#include <vulkan/vulkan.h>

template<>
struct fmt::formatter<VkResult>
{
public:
    constexpr auto parse (format_parse_context& ctx) { return ctx.begin(); }
    template <typename Context>
    constexpr auto format (VkResult vkr, Context& ctx) const {
        return format_to(ctx.out(), "VkResult {}", static_cast<std::underlying_type_t<VkResult>>(vkr));
    }
};
