#pragma once

#include <string>

namespace spurv {

struct Color
{
    float r, g, b, a;
};

Color parseColor(const std::string& color);
Color premultiplied(float r, float g, float b, float a);

} // namespace spurv
