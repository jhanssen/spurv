#pragma once

#include "Layout.h"
#include "TextClasses.h"
#include "Styleable.h"
#include <EventEmitter.h>
#include <Font.h>
#include <TextLine.h>
#include <TextProperty.h>
#include <rope.hpp>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace spurv {

class Cursor;
struct DocumentSelectorInternal;

class Document : public Styleable
{
public:
    using Rope = proj::rope;
    using RopeNode = proj::rope_node;

    Document();
    ~Document();

    void load(const std::filesystem::path& path);
    void load(const std::u16string& data);
    void load(std::u16string&& data);

    // styling
    void setFont(const Font& font);

    void addTextClassAtRange(uint32_t clazz, std::size_t start, std::size_t end);
    void removeTextClassAtRange(uint32_t clazz, std::size_t start, std::size_t end);
    void overwriteTextClassesAtRange(uint32_t clazz, std::size_t start, std::size_t end);

    void clearTextClassesAtRange(std::size_t start, std::size_t end);
    void clearTextClasses();

    enum class Remove { Forward, Backward };
    void remove(Remove mode);

    bool isReady() const;
    EventEmitter<void()>& onReady();

    std::size_t numLines() const;

    TextLine textForLine(std::size_t line) const;
    std::vector<TextLine> textForRange(std::size_t start, std::size_t end);

    std::vector<TextProperty> propertiesForLine(std::size_t line) const;
    std::vector<TextProperty> propertiesForRange(std::size_t start, std::size_t end) const;

protected:
    virtual void updateLayout(const Rect& rect) override;

private:
    Document(Document&&) = delete;
    Document(const Document&) = delete;
    Document& operator=(Document&&) = delete;
    Document& operator=(const Document&) = delete;

private:
    void initialize(std::size_t offset);

    enum class Commit { Finalize, Reinitialize };
    void commit(Commit mode, std::size_t offset = std::numeric_limits<std::size_t>::max());

    void loadChunk(std::u16string&& data);
    void loadComplete();

    void removeSelector(const DocumentSelectorInternal* selector);

    TextProperty propertyForClasses(std::size_t start, std::size_t end, const std::vector<uint32_t>& classes) const;

private:
    Font mFont;
    Layout mLayout;
    Rope mRope;

    enum { ChunkSize = 1000 };
    std::u16string mChunk;
    std::size_t mChunkStart = 0, mChunkOffset = 0;
    std::size_t mDocumentSize = 0, mDocumentLines = 0;

    TextClasses* mTextClasses = nullptr;

    struct TextClassEntry
    {
        std::size_t start;
        std::size_t end;
        std::vector<uint32_t> classes;

        bool operator==(const TextClassEntry& other) const;
        bool operator<(const TextClassEntry& other) const;
    };
    std::vector<TextClassEntry> mTextClassEntries;

    static bool overlaps(const TextClassEntry& t1, const TextClassEntry& t2);
    static bool contains(const TextClassEntry& t1, const TextClassEntry& t2);

    bool mReady = false;
    EventEmitter<void()> mOnReady;

    friend class Cursor;
    friend struct DocumentSelectorInternal;
};

/*
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
*/

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
