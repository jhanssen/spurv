#pragma once

#include "Frame.h"
#include <Document.h>
#include <EventEmitter.h>
#include <Geometry.h>
#include <memory>
#include <cstdint>

namespace spurv {

class View : public Frame
{
public:
    View();
    virtual ~View();

    void setDocument(const std::shared_ptr<Document>& doc);
    const std::shared_ptr<Document> document() const;

    void setActive(bool active);
    bool isActive() const;

    EventEmitter<void(const std::shared_ptr<Document>&)>& onDocumentChanged();

protected:
    virtual void updateLayout(const Rect& rect) override;

private:
    void processDocument();

private:
    std::shared_ptr<Document> mDocument;
    uint64_t mFirstLine = 0;
    bool mActive = false;
    EventEmitter<void(const std::shared_ptr<Document>&)> mOnDocumentChanged;

private:
    View(const View&) = delete;
    View& operator=(const View&) = delete;
};

inline const std::shared_ptr<Document> View::document() const
{
    return mDocument;
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

inline EventEmitter<void(const std::shared_ptr<Document>&)>& View::onDocumentChanged()
{
    return mOnDocumentChanged;
}

} // namespace spurv
