#pragma once

#include <Color.h>
#include "TextStyle.h"
#include <cstddef>

namespace spurv {

struct TextProperty
{
    std::size_t start, end;
    Color foreground = {};
    Color background = {};
    TextStyle style = {};
    int32_t sizeDelta = 0;
};

} // namespace spurv
