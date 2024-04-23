#include "Document.h"
#include <EventLoop.h>
#include <Logger.h>
#include <ThreadPool.h>
#include <simdutf.h>
#include <fmt/core.h>
#include <algorithm>
#include <atomic>
#include <cstdio>

namespace spurv {

struct DocumentSelectorInternal : public std::enable_shared_from_this<DocumentSelectorInternal>
{
    DocumentSelectorInternal(Document* d, std::size_t s, std::size_t e, const std::string& n)
        : document(d), start(s), end(e), name(n)
    {
    }
    ~DocumentSelectorInternal()
    {
        if (document) {
            document->removeSelector(this);
        }
    }

    Document* document;
    std::size_t start, end;
    std::string name;
};

} // namespace spurv

using namespace spurv;

Document::Document()
{
}

Document::~Document()
{
}

void Document::load(const std::filesystem::path& path)
{
    mLayout.reset(Layout::Mode::Chunked);
    mLayout.onReady().connect([this]() {
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
            char32_t utf32buf[32768];
            std::u32string chunk;
            while (!feof(f)) {
                int r = fread(buf + bufOffset, 1, sizeof(buf) - bufOffset, f);
                if (r > 0) {
                    if (encoding == simdutf::unspecified) {
                        encoding = simdutf::autodetect_encoding(buf, r);
                    }
                    switch (encoding) {
                    case simdutf::Latin1: {
                        const std::size_t words = simdutf::convert_latin1_to_utf32(buf, r, utf32buf);
                        chunk = std::u32string(utf32buf, words);
                        break; }
                    case simdutf::UTF8: {
                        const std::size_t utf8len = simdutf::trim_partial_utf8(buf, r);
                        assert(utf8len < sizeof(utf32buf));
                        const std::size_t words = simdutf::convert_utf8_to_utf32(buf, utf8len, utf32buf);
                        if (utf8len != static_cast<std::size_t>(r)) {
                            assert(utf8len < static_cast<std::size_t>(r));
                            memmove(buf, buf + utf8len, r - utf8len);
                            bufOffset = r - utf8len;
                        } else {
                            bufOffset = 0;
                        }
                        chunk = std::u32string(utf32buf, words);
                        break; }
                    case simdutf::UTF16_LE: {
                        const std::size_t utf16len = simdutf::trim_partial_utf16le(reinterpret_cast<char16_t*>(buf), r / sizeof(char16_t));
                        const std::size_t words = simdutf::convert_utf16le_to_utf32(reinterpret_cast<char16_t*>(buf), utf16len, utf32buf);
                        const std::size_t utf16len8 = utf16len * sizeof(char16_t);
                        if (utf16len8 != static_cast<std::size_t>(r)) {
                            assert(utf16len8 < static_cast<std::size_t>(r));
                            memmove(buf, buf + utf16len8, r - utf16len8);
                            bufOffset = r - utf16len8;
                        } else {
                            bufOffset = 0;
                        }
                        chunk = std::u32string(utf32buf, words);
                        break; }
                    case simdutf::UTF16_BE: {
                        const std::size_t utf16len = simdutf::trim_partial_utf16be(reinterpret_cast<char16_t*>(buf), r / sizeof(char16_t));
                        const std::size_t words = simdutf::convert_utf16be_to_utf32(reinterpret_cast<char16_t*>(buf), utf16len, utf32buf);
                        const std::size_t utf16len8 = utf16len * sizeof(char16_t);
                        if (utf16len8 != static_cast<std::size_t>(r)) {
                            assert(utf16len8 < static_cast<std::size_t>(r));
                            memmove(buf, buf + utf16len8, r - utf16len8);
                            bufOffset = r - utf16len8;
                        } else {
                            bufOffset = 0;
                        }
                        chunk = std::u32string(utf32buf, words);
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
                        const std::size_t utf32len8 = r * sizeof(char32_t);
                        chunk = std::u32string(reinterpret_cast<char32_t*>(buf), utf32len);
                        if (utf32len8 != static_cast<std::size_t>(r)) {
                            assert(utf32len8 < static_cast<std::size_t>(r));
                            memmove(buf, buf + utf32len8, r - utf32len8);
                            bufOffset = r - utf32len8;
                        } else {
                            bufOffset = 0;
                        }
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

void Document::load(const std::u32string& data)
{
    mDocumentSize = data.size();
    mRope = Rope(data);
    mLayout.reset(Layout::Mode::Single);
    mLayout.onReady().connect([this]() {
        mOnReady.emit();
    });
    mLayout.calculate(mRope.toString(), mRope.linebreaks());
    initialize(0);
}

void Document::load(std::u32string&& data)
{
    mDocumentSize = data.size();
    mRope = Rope(std::move(data));
    mLayout.reset(Layout::Mode::Single);
    mLayout.onReady().connect([this]() {
        mOnReady.emit();
    });
    mLayout.calculate(mRope.toString(), mRope.linebreaks());
    initialize(0);
}

void Document::loadChunk(std::u32string&& data)
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
            line, ll.start,
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
                l, ll.start,
                hb_buffer_reference(ll.buffer),
                ll.font
            });
    }
    return out;
}

TextProperty Document::propertyForSelector(std::size_t start, std::size_t end, const std::string& selector) const
{
    // ### should parse these up front
    TextProperty prop = { start, end };
    for (auto dit = mQss.cbegin(); dit < mQss.cend(); ++dit) {
        const auto& fragment = dit->first;
        if (fragment.selector() == selector) {
            const auto& block = fragment.block();
            for (auto bit = block.cbegin(); bit != block.cend(); ++bit) {
                const auto& name = bit->first;
                if (name == "background-color") {
                    prop.background = parseColor(bit->second.first);
                } else if (name == "color") {
                    prop.foreground = parseColor(bit->second.first);
                }
            }
            break;
        }
    }
    return prop;
}

std::vector<TextProperty> Document::propertiesForRange(std::size_t start, std::size_t end) const
{
    if (start >= mLayout.numLines() || end > mLayout.numLines() || start > end) {
        return {};
    }

    const auto& sinfo = mLayout.lineAt(start);
    const auto& einfo = mLayout.lineAt(end);

    std::vector<TextProperty> props;
    auto pit = mSelectors.begin();
    const auto pitend = mSelectors.cend();
    while (pit != pitend) {
        const auto& ptr = *pit;
        if (sinfo.start > ptr->end) {
            ++pit;
            continue;
        }
        if (einfo.end < ptr->start) {
            break;
        }

        assert(ptr->start < einfo.end && sinfo.start < ptr->end);
        props.push_back(propertyForSelector(ptr->start, ptr->end, ptr->name));
        ++pit;
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

void Document::setStylesheet(const std::string& qss, StylesheetMode mode)
{
    if (mode == StylesheetMode::Replace) {
        mQss = qss::Document(qss);
    } else {
        mQss += qss::Document(qss);
    }
}

Document::Selector Document::addSelector(std::size_t start, std::size_t end, const std::string& name)
{
    auto intern = std::make_shared<DocumentSelectorInternal>(this, start, end, name);
    // mSelectors.push_back(intern);
    auto it = std::lower_bound(mSelectors.begin(), mSelectors.end(), intern, [](const auto& a, const auto& b) -> bool {
        return a->start < b->start || (a->start == b->start && a->end < b->end);
    });
    mSelectors.insert(it, intern);
    return Document::Selector(std::move(intern));
}

void Document::removeSelector(const DocumentSelectorInternal* selector)
{
    auto it = std::find_if(mSelectors.begin(), mSelectors.end(), [selector](const auto& cand) {
        return cand.get() == selector;
    });
    if (it != mSelectors.end()) {
        mSelectors.erase(it);
    }
}

Document::Selector::Selector(std::shared_ptr<DocumentSelectorInternal>&& intern)
    : internal(std::move(intern))
{
}

Document::Selector::~Selector()
{
}

void Document::Selector::remove()
{
    internal.reset();
}
