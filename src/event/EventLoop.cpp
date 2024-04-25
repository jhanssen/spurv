#include "EventLoop.h"
#include <cassert>

using namespace spurv;

EventLoop* EventLoop::sMainEventLoop = nullptr;
thread_local EventLoop* EventLoop::tEventLoop = nullptr;

namespace spurv {

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

class TimerEvent : public EventLoop::Event
{
public:
    TimerEvent(std::function<void(uint32_t)>&& func)
        : mFunc(std::move(func))
    {
    }

    TimerEvent(TimerEvent&&) = default;
    TimerEvent& operator=(TimerEvent&&) = default;

    void setId(uint32_t id)
    {
        mId = id;
    }

protected:
    virtual void execute() override
    {
        mFunc(mId);
    }
private:
    std::function<void(uint32_t)> mFunc;
    uint32_t mId { 0 };
};

EventLoop::EventLoop()
{
}

EventLoop::~EventLoop()
{
    if (sMainEventLoop == this) {
        sMainEventLoop = nullptr;
    }

    assert(tEventLoop == this || tEventLoop == nullptr);
    tEventLoop = nullptr;
}

void EventLoop::install()
{
    assert(tEventLoop == nullptr);
    tEventLoop = this;
}

void EventLoop::uninstall()
{
    assert(tEventLoop == this);
    tEventLoop = nullptr;
}

void EventLoop::processEvents()
{
    std::vector<std::unique_ptr<Event>> events;
    {
        std::lock_guard lock(mMutex);
        std::swap(events, mEvents);
    }
    for (const auto& ev : events) {
        ev->execute();
    }
}

void EventLoop::executeEvent(Event *event)
{
    event->execute();
}

void EventLoop::post(std::unique_ptr<Event>&& event)
{
    std::lock_guard lock(mMutex);
    mEvents.push_back(std::move(event));
}

void EventLoop::post(std::function<void()>&& event)
{
    post(std::make_unique<FunctionEvent>(std::move(event)));
}

uint32_t EventLoop::startTimer(std::function<void(uint32_t)>&& event, uint64_t timeout, TimerMode mode)
{
    const auto ev = std::make_shared<TimerEvent>(std::move(event));
    const uint32_t id = startTimer(ev, timeout, mode);
    ev->setId(id);
    return id;
}
} // namespace spurv
