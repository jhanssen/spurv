#pragma once

#include <EnumClassBitmask.h>

namespace spurv {

enum class TextStyle {
    Normal = 0x0,
    Bold = 0x1,
    Italic = 0x2,
};

template<>
struct IsEnumBitmask<TextStyle> {
    static constexpr bool enable = true;
};

} // namespace sprurv
