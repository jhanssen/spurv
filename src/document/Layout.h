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

    struct LineInfo
    {
        hb_buffer_t* buffer = nullptr;
        std::size_t start = 0, end = 0;
        Font font = {};
    };
    const LineInfo& lineAt(std::size_t idx) const;
    std::size_t numLines() const;

    EventEmitter<void()>& onReady();

private:
    void clearLines();
    void notifyLines(std::size_t processed);

private:
    Layout(const Layout&) = delete;
    Layout(Layout&&) = delete;
    Layout& operator=(const Layout&) = delete;
    Layout& operator=(Layout&&) = delete;

private:
    EventEmitter<void()> mOnReady;

    Mode mMode = Mode::Single;
    Font mFont = {};
    std::size_t mReceived = 0, mProcessed = 0;
    bool mFinalized = false;

    std::vector<LineInfo> mLines;

    std::shared_ptr<LayoutJob> mJob;

private:
    friend class LayoutJob;
};

inline EventEmitter<void()>& Layout::onReady()
{
    return mOnReady;
}

inline const Layout::LineInfo& Layout::lineAt(std::size_t idx) const
{
    return mLines[idx];
}

inline std::size_t Layout::numLines() const
{
    return mLines.size();
}

} // namespace spurv
