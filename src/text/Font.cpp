#include "Font.h"
#include <fontconfig/fontconfig.h>

class FontConfigHolder
{
public:
    static std::filesystem::path fontFileForPattern(const std::string& name);

private:
    FontConfigHolder() = delete;

private:
    static FcConfig* sConfig;
};

FcConfig* FontConfigHolder::sConfig = nullptr;

std::filesystem::path FontConfigHolder::fontFileForPattern(const std::string& name)
{
    if (sConfig == nullptr) {
        sConfig = FcInitLoadConfigAndFonts();
    }

    auto pattern = FcNameParse(reinterpret_cast<const FcChar8*>(name.c_str()));
    FcConfigSubstitute(sConfig, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    std::filesystem::path fileName;

    FcResult res;
    auto font = FcFontMatch(sConfig, pattern, &res);
    if (font != nullptr) {
        FcChar8* file = nullptr;
        if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch) {
            fileName = reinterpret_cast<char*>(file);
        }
        FcPatternDestroy(font);
    }
    FcPatternDestroy(pattern);

    return fileName;
}

using namespace spurv;

Font::Font(const std::string& name)
    : mFile(FontConfigHolder::fontFileForPattern(name))
{
}
