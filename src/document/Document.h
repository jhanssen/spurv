#pragma once

#include "Layout.h"
#include <EventEmitter.h>
#include <Font.h>
#include <TextLine.h>
#include <rope.hpp>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace spurv {

class Document
{
public:
    using Rope = proj::rope;
    using RopeNode = proj::rope_node;

    Document();
    ~Document();

    void load(const std::filesystem::path& path);
    void load(const std::u32string& data);
    void load(std::u32string&& data);

    void setFont(const Font& font);

    // navigate
    // enum class Navigate
    // {
    //     Forward,
    //     Backward,
    //     Up,
    //     Down
    // };
    // void navigate(

    // insert utf32
    void insert(char32_t uc);

    enum class Remove { Forward, Backward };
    void remove(Remove mode);

    bool isReady() const;
    EventEmitter<void()>& onReady();

    std::size_t numLines() const;
    TextLine lineAt(std::size_t line) const;
    std::vector<TextLine> lineRange(std::size_t start, std::size_t end);

private:
    Document(Document&&) = delete;
    Document(const Document&) = delete;
    Document& operator=(Document&&) = delete;
    Document& operator=(const Document&) = delete;

private:
    void initialize(std::size_t offset);

    enum class Commit { Finalize, Reinitialize };
    void commit(Commit mode, std::size_t offset = std::numeric_limits<std::size_t>::max());

    void loadChunk(std::u32string&& data);
    void loadComplete();

private:
    Font mFont;
    Layout mLayout;
    Rope mRope;

    enum { ChunkSize = 1000 };
    std::u32string mChunk;
    std::size_t mChunkStart = 0, mChunkOffset = 0;
    std::size_t mDocumentSize = 0, mDocumentLines = 0;

    bool mReady = true;
    EventEmitter<void()> mOnReady;
};

inline void Document::insert(char32_t uc)
{
    if (!mReady) {
        return;
    }

    mChunk.insert(mChunk.begin() + (mChunkOffset++), uc);
    ++mDocumentSize;

    if (mChunkStart + mChunkOffset == mDocumentSize) {
        // at the end of the document
        if (mChunkOffset == ChunkSize) {
            commit(Commit::Reinitialize);
        }
    } else if (mChunkOffset == mChunk.size()) {
        commit(Commit::Reinitialize);
    }
}

inline void Document::remove(Remove mode)
{
    if (!mReady) {
        return;
    }
    switch (mode) {
    case Remove::Forward:
        if (mChunkOffset == mChunk.size()) {
            if (mChunkStart + mChunk.size() < mDocumentSize) {
                commit(Commit::Reinitialize, mChunkStart + mChunk.size());
            } else {
                break;
            }
        }
        mChunk.erase(mChunk.begin() + mChunkOffset);
        --mDocumentSize;
        break;
    case Remove::Backward:
        if (mChunkOffset == 0) {
            if (mChunkStart > 0) {
                commit(Commit::Reinitialize, mChunkStart > ChunkSize ? mChunkStart - ChunkSize : 0);
            } else {
                break;
            }
        }
        mChunk.erase(mChunk.begin() + (--mChunkOffset));
        --mDocumentSize;
        break;
    };
}

inline std::size_t Document::numLines() const
{
    return mDocumentLines;
}

inline bool Document::isReady() const
{
    return mReady;
}

inline EventEmitter<void()>& Document::onReady()
{
    return mOnReady;
}

} // namespace spurv
