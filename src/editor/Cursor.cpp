#include "Cursor.h"
#include "View.h"
#include <Logger.h>
#include <TextClasses.h>

using namespace spurv;

static inline uint32_t endClusterForLine(const Layout::LineInfo& line)
{
    if (line.endCluster == line.startCluster + 1) {
        return 0;
    }
    return line.endCluster - line.startCluster;
}

Cursor::Cursor()
{
    setSelector("cursor");
    mTextClass = TextClasses::instance()->registerTextClass("cursor");
}

Cursor::Cursor(const std::shared_ptr<View>& view)
    : Cursor()
{
    setView(view);
}

Cursor::~Cursor()
{
    if (mView) {
        mView->removeStyleableChild(this);
    }
}

void Cursor::setView(const std::shared_ptr<View>& view)
{
    if (mView) {
        mView->removeStyleableChild(this);
        if (mOnDocReady != EventLoop::ConnectKey {}) {
            if (mView->document()) {
                mView->document()->onReady().disconnect(mOnDocReady);
            }
            mOnDocReady = {};
        }
        if (mOnDocChanged != EventLoop::ConnectKey {}) {
            mView->onDocumentChanged().disconnect(mOnDocChanged);
            mOnDocChanged = {};
        }
    }
    mView = view;
    if (mView) {
        mView->addStyleableChild(this);
        if (mView->document()) {
            mOnDocReady = mView->document()->onReady().connect([this]() {
                updatePosition();
            });
        }
        mOnDocChanged = mView->onDocumentChanged().connect([this](const std::shared_ptr<Document>& oldDoc) {
            if (mOnDocReady != EventLoop::ConnectKey {}) {
                if (oldDoc) {
                    oldDoc->onReady().disconnect(mOnDocReady);
                }
                mOnDocReady = {};
            }
            const auto& newDoc = mView->document();
            if (newDoc) {
                mOnDocReady = newDoc->onReady().connect([this]() {
                    updatePosition();
                });
            }
        });
    }
}

void Cursor::setPosition(std::size_t line, uint32_t cluster)
{
    if (!mView) {
        return;
    }
    const auto& layout = mView->document()->mLayout;
    const auto numLines = layout.numLines();
    if (line >= numLines) {
        return;
    }
    const auto& lineInfo = layout.lineAt(line);
    if (lineInfo.startCluster + cluster > lineInfo.endCluster) {
        mLine = line;
        mCluster = endClusterForLine(lineInfo);
    } else {
        mLine = line;
        mCluster = mRetainedCluster = cluster;
    }
}

size_t Cursor::offset() const
{
    if (!mView) {
        return 0;
    }
    const auto& layout = mView->document()->mLayout;
    const auto numLines = layout.numLines();
    if (mLine >= numLines) {
        return 0;
    }
    const auto& lineInfo = layout.lineAt(mLine);
    if (lineInfo.startCluster + mCluster > lineInfo.endCluster) {
        return 0;
    }
    return lineInfo.startCluster + mCluster;
}

void Cursor::setOffset(std::size_t cluster)
{
    if (!mView) {
        return;
    }
    const auto& layout = mView->document()->mLayout;
    const auto numLines = layout.numLines();
    for (std::size_t line = 0; line < numLines; ++line) {
        const auto& lineInfo = layout.lineAt(line);
        if (lineInfo.endCluster >= cluster) {
            mLine = line;
            mCluster = mRetainedCluster = lineInfo.endCluster - cluster;
            return;
        }
    }
    // outside the document?
    if (numLines == 0) {
        mLine = 0;
        mCluster = mRetainedCluster = 0;
    } else {
        mLine = numLines - 1;
        const auto& lineInfo = layout.lineAt(mLine);
        mCluster = mRetainedCluster = lineInfo.endCluster - 1;
    }
}

bool Cursor::navigate(Navigate nav)
{
    if (!mView) {
        return false;
    }
    const auto& layout = mView->document()->mLayout;
    const auto numLines = layout.numLines();
    if (mLine >= numLines) {
        return false;
    }
    const auto& lineInfo = layout.lineAt(mLine);
    if (lineInfo.startCluster + mCluster > lineInfo.endCluster) {
        return false;
    }
    const auto lineEndCluster = endClusterForLine(lineInfo);

    auto relativeCluster = [cluster = &mCluster](const Layout::LineInfo& lineInfo, int32_t dir) -> uint32_t {
        if (!lineInfo.buffer) {
            return *cluster;
        }
        uint32_t highLineCluster = 0;
        uint32_t glyphCount;
        auto glyphInfo = hb_buffer_get_glyph_infos(lineInfo.buffer, &glyphCount);
        for (uint32_t gi = 0; gi < glyphCount; ++gi) {
            if (glyphInfo[gi].cluster == *cluster) {
                if (dir < 0 && static_cast<uint32_t>(abs(dir)) <= gi) {
                    return glyphInfo[gi + dir].cluster;
                } else if (dir >= 0 && gi + dir < glyphCount) {
                    return glyphInfo[gi + dir].cluster;
                }
                return std::numeric_limits<uint32_t>::max();
            } else if (glyphInfo[gi].cluster > highLineCluster) {
                highLineCluster = glyphInfo[gi].cluster;
            }
        }
        if (*cluster == highLineCluster + 1) {
            if (dir < 0) { // at end of line
                if (static_cast<uint32_t>(abs(dir)) <= highLineCluster + 1) {
                    return glyphInfo[highLineCluster + 1 + dir].cluster;
                }
            } else if (dir == 0) {
                return *cluster;
            }
        }
        return std::numeric_limits<uint32_t>::max();
    };

    switch (nav) {
    case Navigate::ClusterForward: {
        auto newCluster = relativeCluster(lineInfo, 1);
        if (newCluster == std::numeric_limits<uint32_t>::max()) {
            // next line?
            if (mLine + 1 < numLines) {
                mLine += 1;
                mCluster = mRetainedCluster = 0;
            }
        } else {
            mCluster = mRetainedCluster = newCluster;
        }
        break; }
    case Navigate::ClusterBackward: {
        auto newCluster = relativeCluster(lineInfo, -1);
        if (newCluster == std::numeric_limits<uint32_t>::max()) {
            // previous line?
            if (mLine > 0) {
                mLine -= 1;
                const auto& newLineInfo = layout.lineAt(mLine);
                mCluster = mRetainedCluster = endClusterForLine(newLineInfo);
            }
        } else {
            mCluster = mRetainedCluster = newCluster;
        }
        break; }
    case Navigate::LineUp:
        if (mLine > 0) {
            mLine -= 1;
            // reset mCluster since relativeCluster refers to it
            mCluster = mRetainedCluster;
            mCluster = relativeCluster(layout.lineAt(mLine), 0);
            if (mCluster == std::numeric_limits<uint32_t>::max()) {
                const auto& newLineInfo = layout.lineAt(mLine);
                mCluster = endClusterForLine(newLineInfo);
            }
        }
        break;
    case Navigate::LineDown:
        if (mLine + 1 < numLines) {
            mLine += 1;
            // reset mCluster since relativeCluster refers to it
            mCluster = mRetainedCluster;
            mCluster = relativeCluster(layout.lineAt(mLine), 0);
            if (mCluster == std::numeric_limits<uint32_t>::max()) {
                const auto& newLineInfo = layout.lineAt(mLine);
                mCluster = endClusterForLine(newLineInfo);
            }
        }
        break;
        break;
    case Navigate::LineStart:
        mCluster = mRetainedCluster = 0;
        break;
    case Navigate::LineEnd:
        mCluster = mRetainedCluster = lineEndCluster;
        break;
    case Navigate::WordForward:
        if (mCluster == lineEndCluster) { // end of line
            if (mLine + 1 < numLines) {
                mLine += 1;
                mCluster = mRetainedCluster = 0;
            }
        } else {
            // find the next word
            const auto& words = lineInfo.wordBreaks;
            auto it = std::find_if(words.begin(), words.end(), [cluster = mCluster](auto wordBreak) -> bool {
                return wordBreak > cluster;
            });
            assert(it != words.end());
            mCluster = mRetainedCluster = *it;
        }
        break;
    case Navigate::WordBackward:
        if (mCluster == 0) { // start of line
            if (mLine > 0) {
                mLine -= 1;
                // now at end of line
                const auto& newLineInfo = layout.lineAt(mLine);
                mCluster = mRetainedCluster = endClusterForLine(newLineInfo);
            }
        } else {
            // find the previous word
            const auto& words = lineInfo.wordBreaks;
            auto it = std::find_if(words.begin(), words.end(), [cluster = mCluster](auto wordBreak) -> bool {
                return wordBreak >= cluster;
            });
            assert(it != words.begin() && it != words.end());
            mCluster = mRetainedCluster = *(it - 1);
        }
        break;
    }

    return false;
}

uint32_t Cursor::globalCluster() const
{
    const auto& layout = mView->document()->mLayout;
    const auto numLines = layout.numLines();
    if (mLine >= numLines) {
        return std::numeric_limits<uint32_t>::max();
    }
    auto c = mCluster;
    if (mLine > 0) {
        const auto& lineInfo = layout.lineAt(mLine - 1);
        c += lineInfo.endCluster;
    }
    return c;
}

void Cursor::setVisible(bool v)
{
    if (v == mVisible) {
        return;
    }
    mVisible = v;

    const auto offset = globalCluster();
    if (offset == std::numeric_limits<uint32_t>::max()) {
        return;
    }
    if (v) {
        mView->document()->addTextClassAtCluster(mTextClass, offset, offset + 1);
    } else {
        mView->document()->removeTextClassAtCluster(mTextClass, offset, offset + 1);
    }
}

void Cursor::updatePosition()
{
    if (mVisible) {
        const auto offset = globalCluster();
        if (offset == std::numeric_limits<uint32_t>::max()) {
            return;
        }
        mView->document()->addTextClassAtCluster(mTextClass, offset, offset + 1);
    }
}

void Cursor::insert(char32_t unicode)
{
    (void)unicode;
}
