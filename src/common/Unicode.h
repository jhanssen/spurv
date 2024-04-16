#pragma once

#include <string>
#include <utility>
#include <cstddef>

namespace spurv {

// users of this function need to deal with CR+LF (which is a single line break) themselves
static inline bool isLineBreak(char32_t ch)
{
    switch (ch) {
    case 0x000A:
    case 0x000B:
    case 0x000C:
    case 0x000D:
    case 0x0085:
    case 0x2028:
    case 0x2029:
        return true;
    default:
        break;
    }
    return false;
}

static inline std::u32string trimLineBreaks(std::u32string&& str)
{
    if (str.empty()) {
        return str;
    }
    std::size_t end = str.size();
    while (end > 0 && isLineBreak(str[end - 1])) {
        --end;
    }
    if (end < str.size()) {
        str.resize(end);
    }
    return str;
}

using Linebreak = std::pair<size_t, char32_t>;

} // namespace spurv
