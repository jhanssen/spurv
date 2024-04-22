#pragma once

#include <array>
#include <cstddef>

namespace spurv {

template<std::size_t Num>
using Vec = std::array<float, Num>;

} // namespace spurv
