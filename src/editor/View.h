#pragma once

#include <Document.h>
#include <memory>
#include <cstdint>

namespace spurv {

class View
{
public:
    View(uint32_t id);
    View(View&&) = default;
    ~View();

    View& operator=(View&&) = default;

    void setDocument(const std::shared_ptr<Document>& doc);
    const std::shared_ptr<Document> document() const;

private:
    void processDocument();

private:
    std::shared_ptr<Document> mDocument;
    uint64_t mFirstLine = 0;
    uint32_t mViewId = 0;

private:
    View(const View&) = delete;
    View& operator=(const View&) = delete;
};

inline const std::shared_ptr<Document> View::document() const
{
    return mDocument;
}

} // namespace spurv
