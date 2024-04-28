#pragma once

#include "Frame.h"
#include <Document.h>
#include <Geometry.h>
#include <memory>
#include <cstdint>

namespace spurv {

class View : public Frame
{
public:
    View();
    View(View&&) = default;
    virtual ~View();

    View& operator=(View&&) = default;

    void setDocument(const std::shared_ptr<Document>& doc);
    const std::shared_ptr<Document> document() const;

    virtual void updateLayout(const Rect& rect) override;
    virtual void setName(const std::string& name) override;

private:
    void processDocument();

private:
    std::shared_ptr<Document> mDocument;
    uint64_t mFirstLine = 0;

private:
    View(const View&) = delete;
    View& operator=(const View&) = delete;
};

inline const std::shared_ptr<Document> View::document() const
{
    return mDocument;
}

inline void View::setName(const std::string& name)
{
    mName = name;
    setSelector(fmt::format("frame > view#{}", name));
}

} // namespace spurv
