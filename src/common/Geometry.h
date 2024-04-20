#pragma once

#include <cstdint>

namespace spurv {

template<typename T>
struct PosT
{
    T x, y;
};

template<typename T>
struct SizeT
{
    T width, height;
};

template<typename XY, typename WH>
struct RectT
{
    XY x, y;
    WH width, height;

    PosT<XY> pos() const { return { x, y }; }
    SizeT<WH> size() const { return { width, height }; }
};

using Pos = PosT<int32_t>;
using PosF = PosT<float>;

using Size = SizeT<uint32_t>;
using SizeF = SizeT<float>;

using Rect = RectT<int32_t, uint32_t>;
using RectF = RectT<float, float>;

} // namespace spurv
