#pragma once

#include <filesystem>
#include <string>

namespace spurv {
std::string readFile(const std::filesystem::path &path, bool *ok = nullptr);
bool writeFile(const std::filesystem::path &path, const std::string &contents);
} // namespace spurv
