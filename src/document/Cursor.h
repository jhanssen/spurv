#pragma once

#include <memory>
#include <cstddef>
#include <cstdint>

namespace spurv {

class Document;

class Cursor
{
public:
    Cursor() = default;
    Cursor(const std::shared_ptr<Document>& doc);
    Cursor(const Cursor&) = default;
    Cursor(Cursor&&) = default;
    ~Cursor();

    Cursor& operator=(const Cursor&) = default;
    Cursor& operator=(Cursor&&) = default;

    void setDocument(const std::shared_ptr<Document>& doc);
    const std::shared_ptr<Document> document() const;

    void setOffset(std::size_t cluster);
    void setLine(std::size_t line, uint32_t cluster = 0);

    std::size_t offset() const;
    std::size_t line() const;
    uint32_t cluster() const;

    enum class Navigate {
        ClusterForward,
        ClusterBackward,
        LineUp,
        LineDown,
        LineStart,
        LineEnd,
        WordForward,
        WordBackward
    };
    bool navigate(Navigate nav);

    void insert(char32_t unicode);

private:
    std::size_t mLine = {};
    uint32_t mCluster = {}, mRetainedCluster = {};

    std::shared_ptr<Document> mDocument = {};
};

inline const std::shared_ptr<Document> Cursor::document() const
{
    return mDocument;
}

inline std::size_t Cursor::line() const
{
    return mLine;
}

inline uint32_t Cursor::cluster() const
{
    return mCluster;
}

} // namespace spurv
