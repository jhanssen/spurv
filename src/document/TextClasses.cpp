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
    if (mNextAvailableClass < mRegisteredClasses.size() && !mRegisteredClasses[mNextAvailableClass].has_value()) {
        mRegisteredClasses[mNextAvailableClass] = qss::Selector(fmt::format("text.{}", name));
        return static_cast<uint32_t>(mNextAvailableClass) + 1;
    } else {
        for (std::size_t r = mNextAvailableClass; r < mRegisteredClasses.size(); ++r) {
            if (!mRegisteredClasses[r].has_value()) {
                mNextAvailableClass = r;
                mRegisteredClasses[r] = qss::Selector(fmt::format("text.{}", name));
                return static_cast<uint32_t>(r) + 1;
            }
        }
        mRegisteredClasses.push_back(qss::Selector(fmt::format("text.{}", name)));
        mNextAvailableClass = mRegisteredClasses.size();
        return static_cast<uint32_t>(mNextAvailableClass);
    }
}

bool TextClasses::unregisterTextClass(uint32_t clazz)
{
    if (clazz == 0) {
        // should never happen
        return false;
    }
    if (clazz < mRegisteredClasses.size() && mRegisteredClasses[clazz - 1].has_value()) {
        mRegisteredClasses[clazz - 1] = {};
        mNextAvailableClass = clazz - 1;
        return true;
    }
    return false;
}

void TextClasses::clearRegisteredTextClasses()
{
    mRegisteredClasses.clear();
    mNextAvailableClass = 0;
}
