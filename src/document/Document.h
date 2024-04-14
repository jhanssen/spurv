#pragma once

#include <rope.hpp>
#include <filesystem>
#include <limits>
#include <string>
#include <EventEmitter.h>

namespace spurv {

class Document
{
public:
    using Rope = proj::rope;

    Document();
    ~Document();

    void load(const std::filesystem::path& path);
    void load(const std::u32string& data);
    void load(std::u32string&& data);

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

private:
    void initialize(std::size_t offset);

    enum class Commit { Finalize, Reinitialize };
    void commit(Commit mode, std::size_t offset = std::numeric_limits<std::size_t>::max());

    void loadChunk(std::u32string&& data);
    void loadFinalize();

private:
    Rope mRope;

    enum { ChunkSize = 1000 };
    std::u32string mChunk;
    std::size_t mChunkStart = 0, mChunkOffset = 0;
    std::size_t mDocumentSize = 0;

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

inline bool Document::isReady() const
{
    return mReady;
}

inline EventEmitter<void()>& Document::onReady()
{
    return mOnReady;
}

} // namespace spurv
