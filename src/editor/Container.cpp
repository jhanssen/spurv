#include "Container.h"

using namespace spurv;

void Container::addFrame(const std::shared_ptr<Frame>& frame)
{
    assert(frame);
    addStyleableChild(frame.get());
    mFrames.insert(frame);
}

void Container::removeFrame(const std::shared_ptr<Frame>& frame)
{
    removeStyleableChild(frame.get());
    mFrames.erase(frame);
}

void Container::updateLayout(const Rect& rect)
{
    (void)rect;
}
