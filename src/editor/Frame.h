#pragma once

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
};

inline Frame::Frame()
{
    setSelector("frame");
}

inline void Frame::updateLayout(const Rect& rect)
{
    (void)rect;
}

} // namespace spurv
