#pragma once

#include <chrono>

namespace spurv {

inline uint64_t timeNow()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

} // namespace spurv
