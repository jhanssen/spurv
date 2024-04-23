#pragma once

#include <Color.h>
#include "TextStyle.h"
#include <cstddef>

namespace spurv {

struct TextProperty
{
    std::size_t start, end;
    Color foreground = { 1.f, 1.f, 1.f, 1.f };
    Color background = {};
    TextStyle style = {};
    int32_t sizeDelta = 0;

    bool operator==(const TextProperty& other) const
    {
        return start == other.start
            && end == other.end
            && foreground == other.foreground
            && background == other.background
            && style == other.style
            && sizeDelta == other.sizeDelta;
    }
};

} // namespace spurv
