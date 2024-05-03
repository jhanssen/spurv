#pragma once

#include <unordered_map>
#include <vector>

namespace spurv {
template <typename Key, typename Value>
std::vector<Key> keys(const std::unordered_map<Key, Value> &container)
{
    std::vector<Key> ret;
    ret.reserve(container.size());
    for (const auto &pair : container) {
        ret.push_back(pair.first);
    }
    return ret;
}
} // namespace spurv
