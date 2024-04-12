#pragma once

#include <Color.h>
#include <optional>

namespace spurv {

struct BoxBorder
{
    float thickness;
    float softness = 2.0f;
};

struct BoxShadow
{
    float offset[2] = { 0.0f, 0.0f };
    float softness = 30.0;
};

struct Box
{
    Color borderColor, backgroundColor;
    float horizontalGrow = 1.0f, verticalGrow = 1.0f;
    std::optional<BoxBorder> border = {};
    std::optional<BoxShadow> shadow = {};
};

} // namespace spurv
