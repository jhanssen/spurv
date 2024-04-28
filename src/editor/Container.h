#pragma once

#include "Frame.h"
#include "View.h"
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
#include <cstddef>

namespace spurv {

class Container : public Frame
{
public:
    Container();
    virtual ~Container() = default;

    void addFrame(const std::shared_ptr<Frame>& frame);
    void removeFrame(const std::shared_ptr<Frame>& frame);

    std::size_t size() const;

    virtual void updateLayout(const Rect& rect) override;
    virtual void setName(const std::string& name) override;

private:
    std::unordered_set<std::shared_ptr<Frame>> mFrames;
};

inline Container::Container()
{
    setSelector("frame > container");
}

inline std::size_t Container::size() const
{
    return mFrames.size();
}

inline void Container::setName(const std::string& name)
{
    mName = name;
    setSelector(fmt::format("frame > container#{}", name));
}

} // namespace spurv
