#pragma once

#include <Renderer.h>
#include <Styleable.h>
#include <string>
#include <fmt/core.h>

namespace spurv {

class View;

class Frame : public Styleable
{
public:
    Frame();
    Frame(Frame&&);
    virtual ~Frame();

    Frame& operator=(Frame&&);

    uint64_t frameNo() const;

    virtual void updateLayout(const Rect& rect) override;

private:
    void extractRenderViewData();

private:
    uint64_t mFrameNo;
    RenderViewData mRenderViewData = {};
    EventLoop::ConnectKey mOnAppliedStylesheetKey = {};

private:
    static uint64_t sFrameNo;
};

inline uint64_t Frame::frameNo() const
{
    return mFrameNo;
}

} // namespace spurv
