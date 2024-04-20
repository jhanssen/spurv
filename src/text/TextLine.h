#pragma once

#include "Font.h"
#include <hb.h>
#include <cstddef>

namespace spurv {

struct TextLine
{
    // remember to call hb_buffer_reference/hb_buffer_destroy if copying this
    std::size_t lineNo = 0;
    hb_buffer_t* buffer = nullptr;
    Font font = {};
};

} // namespace spurv
