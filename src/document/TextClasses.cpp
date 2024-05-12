#include "TextClasses.h"
#include <Formatting.h>

using namespace spurv;

std::unique_ptr<TextClasses> TextClasses::sInstance;

TextClasses* TextClasses::instance()
{
    if (!sInstance) {
        sInstance.reset(new TextClasses);
    }
    return sInstance.get();
}

void TextClasses::destroy()
{
    sInstance.reset();
}

uint32_t TextClasses::registerTextClass(const std::string& name)
{
    auto selector = qss::Selector(fmt::format(".text-{}", name));
    const auto sz = mRegisteredClasses.size();
    auto nextAvailable = sz;
    // see if this class is already registered
    for (std::size_t idx = 0; idx < sz; ++idx) {
        if (mRegisteredClasses[idx].has_value()) {
            if (mRegisteredClasses[idx] == selector) {
                return idx + 1;
            }
        } else if (nextAvailable == sz) {
            nextAvailable = idx;
        }
    }
    if (nextAvailable < sz) {
        mRegisteredClasses[nextAvailable] = std::move(selector);
        return nextAvailable + 1;
    }
    mRegisteredClasses.push_back(std::move(selector));
    return mRegisteredClasses.size();
}

bool TextClasses::unregisterTextClass(uint32_t clazz)
{
    if (clazz == 0) {
        // should never happen
        return false;
    }
    if (clazz < mRegisteredClasses.size() && mRegisteredClasses[clazz - 1].has_value()) {
        mRegisteredClasses[clazz - 1] = {};
        return true;
    }
    return false;
}

void TextClasses::clearRegisteredTextClasses()
{
    mRegisteredClasses.clear();
}
