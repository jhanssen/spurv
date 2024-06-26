#include "Font.h"
#include <fontconfig/fontconfig.h>
#include <mutex>

static std::once_flag initFc;

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
    std::call_once(initFc, []() {
        sConfig = FcInitLoadConfigAndFonts();
    });

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

Font::Font()
{
}

Font::Font(const std::string& name, uint32_t size)
    : mFile(FontConfigHolder::fontFileForPattern(name)), mSize(size)
{
    if (mFile.empty()) {
        return;
    }
    mBlob = hb_blob_create_from_file(mFile.c_str());
    if (mBlob == nullptr) {
        return;
    }
    mFace = hb_face_create(mBlob, 0);
    if (mFace == nullptr) {
        hb_blob_destroy(mBlob);
        mBlob = nullptr;
    }
    mFont = hb_font_create(mFace);
    if (mFont == nullptr) {
        hb_face_destroy(mFace);
        mFace = nullptr;
        hb_blob_destroy(mBlob);
        mBlob = nullptr;
    }
    hb_font_set_scale(mFont, size * 64, size * 64);
}

Font::Font(const Font& other)
    : mFile(other.mFile), mSize(other.mSize)
{
    if (other.mBlob) {
        mBlob = hb_blob_reference(other.mBlob);
    }
    if (other.mFace) {
        mFace = hb_face_reference(other.mFace);
    }
    if (other.mFont) {
        mFont = hb_font_reference(other.mFont);
    }
}

Font::Font(Font&& other)
    : mFile(std::move(other.mFile)), mSize(other.mSize),
      mBlob(other.mBlob), mFace(other.mFace), mFont(other.mFont)
{
    other.mBlob = nullptr;
    other.mFace = nullptr;
    other.mFont = nullptr;
}

Font::~Font()
{
    clear();
}

Font& Font::operator=(const Font& other)
{
    clear();

    mFile = other.mFile;
    mSize = other.mSize;
    if (other.mBlob) {
        mBlob = hb_blob_reference(other.mBlob);
    }
    if (other.mFace) {
        mFace = hb_face_reference(other.mFace);
    }
    if (other.mFont) {
        mFont = hb_font_reference(other.mFont);
    }
    return *this;
}

Font& Font::operator=(Font&& other)
{
    clear();

    mFile = std::move(other.mFile);
    mSize = other.mSize;
    mBlob = other.mBlob;
    other.mBlob = nullptr;
    mFace = other.mFace;
    other.mFace = nullptr;
    mFont = other.mFont;
    other.mFont = nullptr;
    return *this;
}

void Font::clear()
{
    if (mFont != nullptr) {
        hb_font_destroy(mFont);
        mFont = nullptr;
    }
    if (mFace != nullptr) {
        hb_face_destroy(mFace);
        mFace = nullptr;
    }
    if (mBlob != nullptr) {
        hb_blob_destroy(mBlob);
        mBlob = nullptr;
    }
}
