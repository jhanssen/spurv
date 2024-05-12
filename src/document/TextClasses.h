#pragma once

#include <qssselector.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace spurv {

class Document;

class TextClasses
{
public:
    static TextClasses* instance();
    static void destroy();

    uint32_t registerTextClass(const std::string& name);
    bool unregisterTextClass(uint32_t clazz);
    void clearRegisteredTextClasses();

private:
    std::vector<std::optional<qss::Selector>> mRegisteredClasses;

private:
    static std::unique_ptr<TextClasses> sInstance;

    friend class Document;

private:
    TextClasses() = default;
    TextClasses(TextClasses&&) = delete;
    TextClasses(const TextClasses&) = delete;
    TextClasses& operator=(TextClasses&&) = delete;
    TextClasses& operator=(const TextClasses&) = delete;
};

} // namespace spurv
