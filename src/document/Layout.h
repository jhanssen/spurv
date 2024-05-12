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

    void calculate(std::u16string&& data, std::vector<Linebreak>&& linebreaks);
    void finalize();

    struct LineInfo
    {
        hb_buffer_t* buffer = nullptr;
        std::size_t startOffset = 0, endOffset = 0;
        std::size_t startCluster = 0, endCluster = 0;
        std::vector<std::size_t> wordBreaks = {};
        Font font = {};
    };
    const LineInfo& lineAt(std::size_t idx) const;
    std::pair<std::size_t, const LineInfo*> lineForCluster(std::size_t cluster) const;
    std::size_t numLines() const;

    EventEmitter<void()>& onReady();

private:
    void clearLines();
    void notifyLines(std::size_t processed, std::size_t endCluster);

private:
    Layout(const Layout&) = delete;
    Layout(Layout&&) = delete;
    Layout& operator=(const Layout&) = delete;
    Layout& operator=(Layout&&) = delete;

private:
    EventEmitter<void()> mOnReady;

    Mode mMode = Mode::Single;
    Font mFont = {};
    std::size_t mReceived = 0, mProcessed = 0, mEndCluster = 0;
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
