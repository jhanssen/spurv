#pragma once

#include <Color.h>
#include <array>
#include <cstddef>

namespace spurv {

template<std::size_t Num>
using Vec = std::array<float, Num>;

inline Vec<4> colorToVec4(const Color& color)
{
    return Vec<4> { color.r, color.g, color.b, color.a };
}

} // namespace spurv
