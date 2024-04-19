#pragma once

#include <EventEmitter.h>
#include <Font.h>
#include <Unicode.h>
#include <memory>
#include <string>
#include <vector>
#include <hb.h>

namespace spurv {

class LayoutJob;

class Layout
{
public:
    Layout();
    ~Layout();

    void setFont(const Font& font);

    enum class Mode { Single, Chunked };
    void reset(Mode mode);

    void calculate(std::u32string&& data, std::vector<Linebreak>&& linebreaks);
    void finalize();

    EventEmitter<void()>& onReady();

private:
    void clearFont();
    void clearLines();
    void notifyLines();

private:
    Layout(const Layout&) = delete;
    Layout(Layout&&) = delete;
    Layout& operator=(const Layout&) = delete;
    Layout& operator=(Layout&&) = delete;

private:
    EventEmitter<void()> mOnReady;

    Mode mMode = Mode::Single;
    hb_blob_t* mFontBlob = nullptr;
    hb_face_t* mFontFace = nullptr;
    hb_font_t* mFontFont = nullptr;
    bool mFinalized = false;

    struct LineInfo
    {
        hb_buffer_t* buffer = nullptr;
    };
    std::vector<LineInfo> mLines;

    std::shared_ptr<LayoutJob> mJob;

private:
    friend class LayoutJob;
};

inline EventEmitter<void()>& Layout::onReady()
{
    return mOnReady;
}

} // namespace spurv
