#pragma once

#include <cstdint>

namespace spurv {

struct Pos
{
    int32_t x, y;
};

struct Size
{
    uint32_t width, height;
};

struct Rect
{
    int32_t x, y;
    uint32_t width, height;

    Pos pos() const { return { x, y }; }
    Size size() const { return { width, height }; }
};

} // namespace spurv
