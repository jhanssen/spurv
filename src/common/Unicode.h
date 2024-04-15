#pragma once

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

using Linebreak = std::pair<size_t, char32_t>;

} // namespace spurv
