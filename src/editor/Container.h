#pragma once

#include "Frame.h"
#include "View.h"
#include <UnorderedDense.h>
#include <memory>
#include <string>
#include <vector>
#include <cstddef>

namespace spurv {

class Editor;

class Container : public Frame
{
public:
    Container();
    virtual ~Container() = default;

    void addFrame(const std::shared_ptr<Frame>& frame);
    void removeFrame(const std::shared_ptr<Frame>& frame);

    std::size_t size() const;

    virtual void updateLayout(const Rect& rect) override;

private:
    unordered_dense::set<std::shared_ptr<Frame>> mFrames;

    friend class Editor;
};

inline Container::Container()
{
    setSelector("container");
}

inline std::size_t Container::size() const
{
    return mFrames.size();
}

} // namespace spurv
