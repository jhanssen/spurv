#include "Document.h"
#include <EventLoop.h>
#include <Formatting.h>
#include <Logger.h>
#include <ThreadPool.h>
#include <simdutf.h>
#include <fmt/core.h>
#include <algorithm>
#include <atomic>
#include <cstdio>

using namespace spurv;

static inline bool isTextClass(const std::vector<uint32_t>& classes, uint32_t clazz)
{
    return classes.size() == 1 && classes[0] == clazz;
}

static inline bool hasTextClass(const std::vector<uint32_t>& classes, uint32_t clazz)
{
    return std::find(classes.begin(), classes.end(), clazz) != classes.end();
}

static inline void addTextClass(std::vector<uint32_t>& classes, uint32_t clazz)
{
    classes.push_back(clazz);
}

static inline void addTextClass(std::vector<uint32_t>& classes, const std::vector<uint32_t>& newClasses)
{
    classes.reserve(classes.size() + newClasses.size());
    classes.insert(classes.end(), newClasses.begin(), newClasses.end());
}

static inline void removeTextClass(std::vector<uint32_t>& classes, uint32_t clazz)
{
    auto it = std::find(classes.begin(), classes.end(), clazz);
    if (it != classes.end()) {
        classes.erase(it);
    }
}

static inline std::vector<uint32_t> removedTextClass(const std::vector<uint32_t>& classes, uint32_t clazz)
{
    auto it = std::find(classes.begin(), classes.end(), clazz);
    if (it == classes.end()) {
        return classes;
    }
    auto newClasses = classes;
    newClasses.erase(it);
    return newClasses;
}

Document::Document()
    : mTextClasses(TextClasses::instance())
{
    setSelector("document");
}

Document::~Document()
{
}

void Document::updateLayout(const Rect& rect)
{
    (void)rect;
}

void Document::load(const std::filesystem::path& path)
{
    mLayout.reset(Layout::Mode::Chunked);
    mLayout.onReady().connect([this]() {
        mReady = true;
        mDocumentLines = mLayout.numLines();
        mOnReady.emit();
    });
    mRope = Rope();
    mDocumentSize = 0;

    auto loop = EventLoop::eventLoop();
    auto pool = ThreadPool::mainThreadPool();
    auto doc = this;
    pool->post([loop, path, doc]() -> void {
        // the fact that there's no file read api in std::filesystem is fascinating

        simdutf::encoding_type encoding = {};
        FILE* f = fopen(path.c_str(), "r");
        if (f) {
            std::size_t bufOffset = 0;
            char buf[32768];
            char16_t utf16buf[32768];
            std::u16string chunk;
            while (!feof(f)) {
                int r = fread(buf + bufOffset, 1, sizeof(buf) - bufOffset, f);
                if (r > 0) {
                    if (encoding == simdutf::unspecified) {
                        encoding = simdutf::autodetect_encoding(buf, r);
                    }
                    switch (encoding) {
                    case simdutf::Latin1: {
                        const std::size_t words = simdutf::convert_latin1_to_utf16(buf, r, utf16buf);
                        chunk = std::u16string(utf16buf, words);
                        break; }
                    case simdutf::UTF8: {
                        const std::size_t utf8len = simdutf::trim_partial_utf8(buf, r);
                        assert(utf8len < sizeof(utf16buf));
                        const std::size_t words = simdutf::convert_utf8_to_utf16(buf, utf8len, utf16buf);
                        if (utf8len != static_cast<std::size_t>(r)) {
                            assert(utf8len < static_cast<std::size_t>(r));
                            memmove(buf, buf + utf8len, r - utf8len);
                            bufOffset = r - utf8len;
                        } else {
                            bufOffset = 0;
                        }
                        chunk = std::u16string(utf16buf, words);
                        break; }
                    case simdutf::UTF16_BE: {
                        // assume we're a little endian system, flip the bytes
                        // ### could probably optimize this
                        const std::size_t utf16len = r / sizeof(char16_t);
                        for (std::size_t n = 0; n < utf16len; ++n) {
                            *(reinterpret_cast<uint16_t*>(buf) + n) = __builtin_bswap16(*(reinterpret_cast<uint16_t*>(buf) + n));
                        }
                        [[ fallthrough ]]; }
                    case simdutf::UTF16_LE: {
                        const std::size_t utf16len = r / sizeof(char16_t);
                        const std::size_t utf16len8 = utf16len * sizeof(char16_t);
                        chunk = std::u16string(reinterpret_cast<char16_t*>(buf), utf16len);
                        if (utf16len8 != static_cast<std::size_t>(r)) {
                            assert(utf16len8 < static_cast<std::size_t>(r));
                            memmove(buf, buf + utf16len8, r - utf16len8);
                            bufOffset = r - utf16len8;
                        } else {
                            bufOffset = 0;
                        }
                        break; }
                    case simdutf::UTF32_BE: {
                        // assume we're a little endian system, flip the bytes
                        // ### could probably optimize this
                        const std::size_t utf32len = r / sizeof(char32_t);
                        for (std::size_t n = 0; n < utf32len; ++n) {
                            *(reinterpret_cast<uint32_t*>(buf) + n) = __builtin_bswap32(*(reinterpret_cast<uint32_t*>(buf) + n));
                        }
                        [[ fallthrough ]]; }
                    case simdutf::UTF32_LE: {
                        const std::size_t utf32len = r / sizeof(char32_t);
                        const std::size_t utf32len8 = utf32len * sizeof(char32_t);
                        const std::size_t words = simdutf::convert_utf32_to_utf16(reinterpret_cast<char32_t*>(buf), utf32len, utf16buf);
                        if (utf32len8 != static_cast<std::size_t>(r)) {
                            assert(utf32len8 < static_cast<std::size_t>(r));
                            memmove(buf, buf + utf32len8, r - utf32len8);
                            bufOffset = r - utf32len8;
                        } else {
                            bufOffset = 0;
                        }
                        chunk = std::u16string(utf16buf, words);
                        break; }
                    case simdutf::unspecified:
                        spdlog::info("No text encoding detected");
                        break;
                    }

                    if (!chunk.empty()) {
                        loop->post([doc, chunk = std::move(chunk)]() mutable -> void {
                            doc->loadChunk(std::move(chunk));
                        });
                        chunk.clear();
                    }
                }
            }
            fclose(f);
        } else {
            spdlog::info("Unable to open file {}", path);
        }
        loop->post([doc]() -> void {
            doc->loadComplete();
        });
    });
}

void Document::load(const std::u16string& data)
{
    mDocumentSize = data.size();
    mRope = Rope(data);
    mLayout.reset(Layout::Mode::Single);
    mLayout.onReady().connect([this]() {
        mReady = true;
        mDocumentLines = mLayout.numLines();
        mOnReady.emit();
    });
    mLayout.calculate(mRope.toString(), mRope.linebreaks());
    initialize(0);
}

void Document::load(std::u16string&& data)
{
    mDocumentSize = data.size();
    mRope = Rope(std::move(data));
    mLayout.reset(Layout::Mode::Single);
    mLayout.onReady().connect([this]() {
        mReady = true;
        mDocumentLines = mLayout.numLines();
        mOnReady.emit();
    });
    mLayout.calculate(mRope.toString(), mRope.linebreaks());
    initialize(0);
}

void Document::loadChunk(std::u16string&& data)
{
    mDocumentSize += data.size();
    mRope.append(data);
    mLayout.calculate(std::move(data), mRope.lastLinebreaks());
}

void Document::loadComplete()
{
    spdlog::info("completed loading doc {}", mRope.length());
    spdlog::info("- linebreaks {}", mRope.linebreaks());
    mLayout.finalize();
    initialize(0);
}

void Document::initialize(std::size_t offset)
{
    // if we have a valid rope, take out a chunk
    if (offset < mRope.length()) {
        auto handle = mRope.remove(offset, std::min<std::size_t>(mRope.length(), ChunkSize));
        mChunk = handle->treeToString();
        mChunkStart = offset;
        mChunkOffset = 0;
    } else if (offset > 0 && offset == mRope.length()) {
        // end of document
        mChunk = {};
        mChunkStart = offset;
        mChunkOffset = 0;
    } else {
        assert(offset == 0 && mRope.length() == 0);
        // no valid rope, empty document
        mChunk = {};
        mChunkStart = mChunkOffset = 0;
    }
    spdlog::info("initialize doc {} {}", mChunkStart, mChunk.size());
}

void Document::commit(Commit mode, std::size_t offset)
{
    mRope.insert(mChunkStart, std::move(mChunk));
    if (mode == Commit::Reinitialize) {
        initialize(offset != std::numeric_limits<std::size_t>::max() ? offset : mChunkStart);
    } else {
        assert(offset == std::numeric_limits<std::size_t>::max());
        mChunk = {};
        mChunkStart = mChunkOffset = 0;
    }
}

TextLine Document::textForLine(std::size_t line) const
{
    if (line < mLayout.numLines()) {
        const auto& ll = mLayout.lineAt(line);
        return {
            line, ll.startOffset,
            hb_buffer_reference(ll.buffer),
            ll.font
        };
    }
    return {
        static_cast<std::size_t>(0),
        static_cast<std::size_t>(0),
        nullptr,
        Font {}
    };
}

std::vector<TextLine> Document::textForRange(std::size_t start, std::size_t end)
{
    if (start >= mLayout.numLines() || end > mLayout.numLines() || start > end) {
        return {};
    }
    if (start == end) {
        return { textForLine(start) };
    }
    std::vector<TextLine> out;
    out.reserve(end - start);
    for (std::size_t l = start; l < end; ++l) {
        const auto& ll = mLayout.lineAt(l);
        out.push_back({
                l, ll.startOffset,
                hb_buffer_reference(ll.buffer),
                ll.font
            });
    }
    return out;
}

TextProperty Document::propertyForClasses(std::size_t start, std::size_t end, const std::vector<uint32_t>& classes) const
{
    // ### should parse these up front
    TextProperty prop = { start, end };
    for (auto dit = mMergedQss.cbegin(); dit < mMergedQss.cend(); ++dit) {
        const auto& fragment = dit->first;
        for (auto clazz : classes) {
            const auto& maybeSelector = mTextClasses->mRegisteredClasses[clazz - 1];
            assert(maybeSelector.has_value());
            if (Styleable::isGeneralizedFrom(maybeSelector.value(), fragment.selector())) {
                const auto& block = fragment.block();
                for (auto bit = block.cbegin(); bit != block.cend(); ++bit) {
                    // ### should extract the relevant code that handles these in Styleable.cpp
                    // into a function that's callable from both there and here
                    const auto& name = bit->first;
                    if (name == "background-color") {
                        auto color = parseColor(bit->second.first);
                        if (color.has_value()) {
                            prop.background = *color;
                        }
                    } else if (name == "color") {
                        auto color = parseColor(bit->second.first);
                        if (color.has_value()) {
                            prop.foreground = *color;
                        }
                    }
                }
                break;
            }
        }
    }
    return prop;
}

bool Document::TextClassEntry::operator==(const Document::TextClassEntry& other) const
{
    return start == other.start && end == other.end;
}

bool Document::TextClassEntry::operator<(const Document::TextClassEntry& other) const
{
    return start < other.start || (start == other.start && end < other.end);
}

bool Document::overlaps(const Document::TextClassEntry& t1, const Document::TextClassEntry& t2)
{
    return t1.start < t2.end && t2.start < t1.end;
}

bool Document::contains(const Document::TextClassEntry& t1, const Document::TextClassEntry& t2)
{
    return t2.start >= t1.start && t2.end <= t1.end;
}

std::vector<TextProperty> Document::propertiesForRange(std::size_t start, std::size_t end) const
{
    if (start >= mLayout.numLines() || end > mLayout.numLines() || start > end) {
        return {};
    }

    const auto& sinfo = mLayout.lineAt(start);
    const auto& einfo = mLayout.lineAt(end > start ? end - 1 : start);

    const std::size_t startCluster = sinfo.startCluster;
    const std::size_t endCluster = einfo.endCluster + 1;

    TextClassEntry findClass = {
        startCluster, endCluster, {}
    };

    std::vector<TextProperty> props;

    auto it = std::lower_bound(mTextClassEntries.begin(), mTextClassEntries.end(), findClass);
    // find the first entry that goes before us
    while (it != mTextClassEntries.begin()) {
        if (!overlaps(*(it - 1), findClass))
            break;
        --it;
    }
    while (it != mTextClassEntries.end() && overlaps(*it, findClass)) {
        props.push_back(propertyForClasses(it->start, it->end, it->classes));
        ++it;
    }

    return props;
}

std::vector<TextProperty> Document::propertiesForLine(std::size_t line) const
{
    return propertiesForRange(line, line);
}

void Document::setFont(const Font& font)
{
    mLayout.setFont(font);
}

void Document::emitPropertiesChanged(std::size_t start, std::size_t end)
{
    assert(start < end);
    const auto [ startLine, startLineInfo ] = mLayout.lineForCluster(start);
    const auto [ endLine, endLineInfo ] = mLayout.lineForCluster(end);
    assert(startLineInfo != nullptr && endLineInfo != nullptr);
    (void)startLineInfo;
    (void)endLineInfo;
    mOnPropertiesChanged.emit(startLine, endLine);
}

void Document::addTextClassAtCluster(uint32_t clazz, std::size_t start, std::size_t end)
{
    assert(start < end);
    TextClassEntry newClass = {
        start, end, { clazz }
    };
    auto it = std::lower_bound(mTextClassEntries.begin(), mTextClassEntries.end(), newClass);
    if (it == mTextClassEntries.end()) {
        mTextClassEntries.push_back(newClass);
    } else {
        if (it->start == start && it->end == end) {
            addTextClass(it->classes, clazz);
        } else {
            mTextClassEntries.insert(it, newClass);
        }
    }
    emitPropertiesChanged(start, end);
}

void Document::removeTextClassAtCluster(uint32_t clazz, std::size_t start, std::size_t end)
{
    assert(start < end);
    TextClassEntry findClass = {
        start, end, { clazz }
    };
    auto it = std::lower_bound(mTextClassEntries.begin(), mTextClassEntries.end(), findClass);
    // find the first entry that goes before us
    while (it != mTextClassEntries.begin()) {
        if (!overlaps(*(it - 1), findClass))
            break;
        --it;
    }
    // we may have to split at the start
    if (it->start < start) {
        assert(it->end > start);
        if (hasTextClass(it->classes, clazz)) {
            // split
            TextClassEntry newClass = {
                start,
                it->end,
                removedTextClass(it->classes, clazz)
            };
            it = mTextClassEntries.insert(it, newClass);
            (it - 1)->end = start;
            ++it;
        }
    }
    // remove clazz from all entries up until end
    while (it != mTextClassEntries.end()) {
        if (it->start > end) {
            break;
        }
        assert(overlaps(*it, findClass));
        if (end >= it->end) {
            assert(contains(findClass, *it));
            removeTextClass(it->classes, clazz);
        } else {
            // this entry has an end that goes beyond our end, we might have to split
            if (hasTextClass(it->classes, clazz)) {
                // if the previous entry equals us then we don't have to split
                if (it != mTextClassEntries.begin() &&
                    (it - 1)->start == it->start &&
                    (it - 1)->end == end) {
                    addTextClass((it - 1)->classes, removedTextClass(it->classes, clazz));
                } else {
                    // we need to split
                    TextClassEntry newClass = {
                        it->start,
                        end,
                        removedTextClass(it->classes, clazz)
                    };
                    // put it past the inserted value, which should be the value
                    // we're currently looking at
                    assert(it == mTextClassEntries.begin() || *(it - 1) < newClass);

                    it = mTextClassEntries.insert(it, newClass) + 1;
                    it->start = end + 1;
                    assert(it->start < it->end);
                }
            }
        }
        ++it;
    }
    emitPropertiesChanged(start, end);
}

void Document::overwriteTextClassesAtCluster(uint32_t clazz, std::size_t start, std::size_t end)
{
    assert(start < end);
    TextClassEntry findClass = {
        start, end, { clazz }
    };
    auto it = std::lower_bound(mTextClassEntries.begin(), mTextClassEntries.end(), findClass);
    // find the first entry that goes before us
    while (it != mTextClassEntries.begin()) {
        if (!overlaps(*(it - 1), findClass))
            break;
        --it;
    }
    // we may have to split at the start
    if (it->start < start) {
        assert(it->end > start);
        if (!isTextClass(it->classes, clazz)) {
            // split
            TextClassEntry newClass = {
                start,
                it->end,
                { clazz }
            };
            it = mTextClassEntries.insert(it, newClass);
            (it - 1)->end = start;
            ++it;
        }
    }
    while (it != mTextClassEntries.end()) {
        if (it->start > end) {
            break;
        }
        assert(overlaps(*it, findClass));
        if (end >= it->end) {
            assert(contains(findClass, *it));
            it->classes = { clazz };
        } else {
            // this entry has an end that goes beyond our end, we might have to split
            if (!isTextClass(it->classes, clazz)) {
                // if the previous entry equals us then we don't have to split
                if (it != mTextClassEntries.begin() &&
                    (it - 1)->start == it->start &&
                    (it - 1)->end == end) {
                    (it - 1)->classes = { clazz };
                } else {
                    // we need to split
                    TextClassEntry newClass = {
                        it->start,
                        end,
                        { clazz }
                    };
                    // put it past the inserted value, which should be the value
                    // we're currently looking at
                    assert(it == mTextClassEntries.begin() || *(it - 1) < newClass);

                    it = mTextClassEntries.insert(it, newClass) + 1;
                    it->start = end + 1;
                    assert(it->start < it->end);
                }
            }
        }
        ++it;
    }
    emitPropertiesChanged(start, end);
}

void Document::clearTextClassesAtCluster(std::size_t start, std::size_t end)
{
    assert(start < end);
    TextClassEntry findClass = {
        start, end, {}
    };
    auto it = std::lower_bound(mTextClassEntries.begin(), mTextClassEntries.end(), findClass);
    // find the first entry that goes before us
    while (it != mTextClassEntries.begin()) {
        if (!overlaps(*(it - 1), findClass))
            break;
        --it;
    }
    if (it->start < start) {
        assert(it->end > start);

        // adjust end
        it->end = start;
        ++it;
    }
    while (it != mTextClassEntries.end()) {
        if (it->start > end) {
            break;
        }
        assert(overlaps(*it, findClass));
        if (end >= it->end) {
            assert(contains(findClass, *it));
            it = mTextClassEntries.erase(it);
        } else {
            // this entry has an end that goes beyond our end, adjust start
            it->start = end;
            assert(it->start < it->end);
            ++it;
        }
    }
    emitPropertiesChanged(start, end);
}

void Document::clearTextClasses()
{
    mTextClassEntries.clear();
    emitPropertiesChanged(0, std::numeric_limits<std::size_t>::max());
}
