#pragma once

#include <Color.h>
#include <Result.h>
#include <Unicode.h>
#include <fmt/core.h>
#include <fmt/color.h>
#include <volk.h>
#include <algorithm>
#include <filesystem>
#include <cstdint>

constexpr auto vectorColor = fmt::terminal_color::bright_yellow;

template<>
struct fmt::formatter<VkResult>
{
public:
    constexpr auto parse (format_parse_context& ctx) { return ctx.begin(); }
    template <typename Context>
    constexpr auto format (VkResult vkr, Context& ctx) const {
        return format_to(ctx.out(), "VkResult {}", static_cast<int32_t>(vkr));
    }
};

template<>
struct fmt::formatter<std::filesystem::path> : formatter<std::string_view>
{
    template <typename Context>
    constexpr auto format(const std::filesystem::path& path, Context& ctx) const {
        return formatter<std::string_view>::format(path.string(), ctx);
    }
};

template<>
struct fmt::formatter<spurv::Linebreak>
{
public:
    constexpr auto parse (format_parse_context& ctx) { return ctx.begin(); }
    template <typename Context>
    constexpr auto format (spurv::Linebreak lb, Context& ctx) const {
        return format_to(ctx.out(), "LB idx={} uc={:#06x}", lb.first, static_cast<uint32_t>(lb.second));
    }
};

template<typename T>
struct fmt::formatter<spurv::Result<T>>
{
public:
    constexpr auto parse (format_parse_context& ctx) { return ctx.begin(); }
    template <typename Context>
    constexpr auto format (spurv::Result<T> r, Context& ctx) const {
        if (r.ok()) {
            return format_to(ctx.out(), "{}", *r);
        }
        return format_to(ctx.out(), "Error {}", r.error().message);
    }
};

template<>
struct fmt::formatter<spurv::Color>
{
public:
    constexpr auto parse (format_parse_context& ctx) { return ctx.begin(); }
    template <typename Context>
    constexpr auto format (spurv::Color c, Context& ctx) const {
        return format_to(ctx.out(), "Color r={:.2f} g={:.2f} b={:.2f} a={:.2f}", c.r, c.g, c.b, c.a);
    }
};

template<typename T>
struct fmt::formatter<std::vector<T>>
{
public:
    constexpr auto parse (format_parse_context& ctx) { return ctx.begin(); }
    template <typename Context>
    constexpr auto format (const std::vector<T>& vec, Context& ctx) const {
        auto it = ctx.out();
        it = format_to(it, "std::vector size={} [ ", vec.size());
        // first 100 elements?
        const auto num = std::min<std::size_t>(vec.size(), 100);
        for (std::size_t n = 0; n < num; ++n) {
            if (n + 1 < num) {
                it = format_to(it, fg(vectorColor), "{}", vec[n]);
                it = format_to(it, ", ");
            } else {
                it = format_to(it, fg(vectorColor), "{}", vec[n]);
            }
        }
        if (num < vec.size()) {
            it = format_to(it, ", ");
            it = format_to(it, fg(vectorColor), "...");
            it = format_to(it, "]");
        } else {
            it = format_to(it, " ]");
        }
        return it;
    }
};
