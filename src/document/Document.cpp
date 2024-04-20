#include "Document.h"
#include <EventLoop.h>
#include <Logger.h>
#include <ThreadPool.h>
#include <simdutf.h>
#include <fmt/core.h>
#include <algorithm>
#include <cstdio>

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

TextLine Document::lineAt(std::size_t line) const
{
    if (line < mLayout.numLines()) {
        const auto& ll = mLayout.lineAt(line);
        return {
            hb_buffer_reference(ll.buffer),
            ll.font
        };
    }
    return {
        nullptr,
        Font {}
    };
}

std::vector<TextLine> Document::lineRange(std::size_t start, std::size_t end)
{
    if (start >= mLayout.numLines() || end > mLayout.numLines() || start > end) {
        return {};
    }
    if (start == end) {
        return { lineAt(start) };
    }
    std::vector<TextLine> out;
    out.reserve(end - start);
    for (std::size_t l = start; l < end; ++l) {
        const auto& ll = mLayout.lineAt(l);
        out.push_back({
                hb_buffer_reference(ll.buffer),
                ll.font
            });
    }
    return out;
}

void Document::setFont(const Font& font)
{
    mLayout.setFont(font);
}
