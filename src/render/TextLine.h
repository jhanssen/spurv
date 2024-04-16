#pragma once

#include <string>
#include <cstdint>

namespace spurv {

struct TextLine {
    std::size_t lineNo;
    std::u32string text;
};

} // namespace spurv
