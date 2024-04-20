#pragma once

#include <hb.h>
#include <filesystem>
#include <string>
#include <cstdint>

namespace spurv {

class Font
{
public:
    Font();
    Font(const std::string& name, uint32_t size);
    Font(const Font& other);
    Font(Font&& other);
    ~Font();

    Font& operator=(const Font& other);
    Font& operator=(Font&& other);
    bool operator==(const Font& other) const;

    bool isValid() const { return mFont != nullptr; }
    const std::filesystem::path& file() const { return mFile; }
    hb_font_t* font() const { return mFont; }

    void clear();

private:
    std::filesystem::path mFile = {};
    hb_blob_t* mBlob = nullptr;
    hb_face_t* mFace = nullptr;
    hb_font_t* mFont = nullptr;
};

inline bool Font::operator==(const Font& other) const
{
    // ### should this consider mFile instead?
    return mFont == other.mFont;
}

}
