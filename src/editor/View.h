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

    void setActive(bool active);
    bool isActive() const;

private:
    void processDocument();

private:
    std::shared_ptr<Document> mDocument;
    uint64_t mFirstLine = 0;
    bool mActive = false;

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
    mutableSelector()[0].id(name);
}

inline void View::setActive(bool active)
{
    if (active == mActive) {
        return;
    }
    if (active) {
        mutableSelector()[0].when("active");
    } else {
        mutableSelector()[0].nowhen("active");
    }
    mActive = active;
}

inline bool View::isActive() const
{
    return mActive;
}

} // namespace spurv
