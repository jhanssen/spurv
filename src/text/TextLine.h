#pragma once

#include "Font.h"
#include <hb.h>

namespace spurv {

struct TextLine
{
    // remember to call hb_buffer_reference/hb_buffer_destroy if copying this
    hb_buffer_t* buffer = nullptr;
    Font font = {};
};

} // namespace spurv
