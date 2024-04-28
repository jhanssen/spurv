#pragma once

#include <Geometry.h>
#include <Styleable.h>
#include <string>
#include <fmt/core.h>

namespace spurv {

class View;

class Frame : public Styleable
{
public:
    Frame();
    virtual ~Frame() = default;

    virtual void setName(const std::string& name) override;

    virtual void updateLayout(const Rect& rect) = 0;
};

inline Frame::Frame()
{
    setSelector("frame");
}

inline void Frame::setName(const std::string& name)
{
    mutableSelector()[0].id(name);
}

} // namespace spurv
