#pragma once

#include <filesystem>
#include <string>

namespace spurv {

class Font
{
public:
    Font();
    Font(const std::string& name);
    ~Font() = default;

    bool isValid() const { return !mFile.empty(); }
    const std::filesystem::path& file() const { return mFile; }

private:
    std::filesystem::path mFile = {};
};

}
