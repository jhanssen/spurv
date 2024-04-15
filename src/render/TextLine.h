#pragma once

#include <string>
#include <cstdint>

namespace spurv {

struct TextLine {
    uint32_t lineNo;
    std::u32string text;
};

} // namespace spurv
