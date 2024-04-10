#include "EventLoop.h"

using namespace spurv;

EventLoop* EventLoop::sMainEventLoop = nullptr;

class FunctionEvent : public EventLoop::Event
{
public:
    FunctionEvent(std::function<void()>&& func)
        : mFunc(std::move(func))
    {
    }

    FunctionEvent(FunctionEvent&&) = default;
    FunctionEvent& operator=(FunctionEvent&&) = default;

protected:
    virtual void execute() override
    {
        mFunc();
    }

private:
    std::function<void()> mFunc;
};

EventLoop::EventLoop()
{
    if (sMainEventLoop == nullptr) {
        sMainEventLoop = this;
    }
}

EventLoop::~EventLoop()
{
    if (sMainEventLoop == this) {
        sMainEventLoop = nullptr;
    }
}

void EventLoop::post(std::function<void()>&& event)
{
    post(std::make_unique<FunctionEvent>(std::move(event)));
}

void EventLoop::post_internal(std::unique_ptr<Event>&& event)
{
    std::lock_guard lock(mMutex);
    mEvents.push_back(std::move(event));
}

bool EventLoop::run_internal()
{
    std::vector<std::unique_ptr<Event>> events;
    {
        std::lock_guard lock(mMutex);
        if (mStopped)
            return false;
        std::swap(events, mEvents);
    }
    for (const auto& ev : events) {
        ev->execute();
    }
    return true;
}

void EventLoop::stop_internal()
{
    std::lock_guard lock(mMutex);
    mStopped = true;
}
