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
    Frame(Frame&&) = default;
    virtual ~Frame() = default;

    Frame& operator=(Frame&&) = default;

    virtual void updateLayout(const Rect& rect) override;

private:
    void extractRenderViewData();

private:
    RenderViewData mRenderViewData = {};

private:
    static uint64_t sFrameNo;
};

} // namespace spurv
