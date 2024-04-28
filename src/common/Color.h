#pragma once

#include <optional>
#include <string>

namespace spurv {

struct Color
{
    float r, g, b, a;

    bool operator==(const Color& other) const
    {
        // ### should do something for float compare
        return r == other.r
            && g == other.g
            && b == other.b
            && a == other.a;
    }
};

std::optional<Color> parseColor(const std::string& color);
Color premultiplied(float r, float g, float b, float a);

} // namespace spurv
