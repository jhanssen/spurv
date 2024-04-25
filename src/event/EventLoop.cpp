#include "EventLoop.h"
#include <uv.h>
#include <cassert>

using namespace spurv;

EventLoop* EventLoop::sMainEventLoop = nullptr;
thread_local EventLoop* EventLoop::tEventLoop = nullptr;

namespace spurv {
struct EventLoopImplUv : public EventLoop::EventLoopImpl
{
    EventLoopImplUv(EventLoop* l);
    ~EventLoopImplUv();

    virtual void *handle() override;
    virtual void post() override;
    virtual void startTimer(uint32_t id, const std::shared_ptr<EventLoop::Event>& event, uint64_t timeout, EventLoop::TimerMode mode) override;
    virtual void stopTimer(uint32_t id) override;
    virtual void stop() override;

    static void processPost(uv_async_t* handle);
    static void processTimer(uv_timer_t* handle);

    EventLoop* loop;
    uv_loop_t uvloop;
    uv_async_t uvpost;
    struct UvTimer
    {
        uint32_t id;
        // holds the event shared_ptr alive for as long as
        // the uv_timer_t holds the raw pointer
        std::shared_ptr<EventLoop::Event> event;
        uv_timer_t timer;
    };
    std::vector<UvTimer> uvtimers;
};

EventLoopImplUv::EventLoopImplUv(EventLoop* l)
    : EventLoopImpl(l)
{
}

EventLoopImplUv::~EventLoopImplUv()
{
    uv_loop_close(&uvloop);
}

void *EventLoopImplUv::handle()
{
    return &uvloop;
}

void EventLoopImplUv::post()
{
    uv_async_send(&uvpost);
}

void EventLoopImplUv::startTimer(uint32_t id, const std::shared_ptr<EventLoop::Event>& event, uint64_t timeout, EventLoop::TimerMode mode)
{
    uvtimers.push_back(EventLoopImplUv::UvTimer { id, event, {} });
    auto timer = &uvtimers.back().timer;
    uv_timer_init(&uvloop, timer);
    timer->data = event.get();
    uv_timer_start(timer, EventLoopImplUv::processTimer, timeout, mode == EventLoop::TimerMode::SingleShot ? 0 : 1);
}

void EventLoopImplUv::stopTimer(uint32_t id)
{
    auto uvit = uvtimers.begin();
    const auto uvend = uvtimers.end();
    while (uvit != uvend) {
        if (uvit->id == id) {
            uv_timer_stop(&uvit->timer);
            uv_close(reinterpret_cast<uv_handle_t*>(&uvit->timer), nullptr);
            uvtimers.erase(uvit);
            break;
        }
        ++uvit;
    }
}

void EventLoopImplUv::stop()
{
    uv_stop(&uvloop);
}

void EventLoopImplUv::processPost(uv_async_t* handle)
{
    EventLoop* loop = static_cast<EventLoop*>(handle->data);
    loop->processEvents();
}

void EventLoopImplUv::processTimer(uv_timer_t* handle)
{
    EventLoop::Event* event = static_cast<EventLoop::Event*>(handle->data);
    event->execute();
}

} // namespace spurv

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
    mImpl.reset();
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

bool EventLoop::processEvents()
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

int32_t EventLoop::run()
{
    assert(tEventLoop == this);
    assert(!mImpl);
    EventLoopImplUv* uvImpl = new EventLoopImplUv(this);
    mImpl.reset(uvImpl);
    uv_loop_init(&uvImpl->uvloop);
    uv_loop_set_data(&uvImpl->uvloop, this);
    uv_async_init(&uvImpl->uvloop, &uvImpl->uvpost, EventLoopImplUv::processPost);
    uvImpl->uvpost.data = this;

    // give events a chance to run
    processEvents();

    uv_run(&uvImpl->uvloop, UV_RUN_DEFAULT);
    return mExitCode;
}

void EventLoop::stop(int32_t exitCode)
{
    assert(tEventLoop == this || tEventLoop == nullptr);
    mExitCode = exitCode;
    std::lock_guard lock(mMutex);
    mStopped = true;
    if (mImpl) {
        mImpl->stop();
    }
}

void EventLoop::post(std::unique_ptr<Event>&& event)
{
    std::lock_guard lock(mMutex);
    mEvents.push_back(std::move(event));
    if (mImpl) {
        mImpl->post();
    }
}

void EventLoop::post(std::function<void()>&& event)
{
    post(std::make_unique<FunctionEvent>(std::move(event)));
}

uint32_t EventLoop::startTimer(const std::shared_ptr<Event>& event, uint64_t timeout, TimerMode mode)
{
    std::lock_guard lock(mMutex);
    auto id = ++mNextTimerId;
    mTimers.push_back({
            id,
            timeout,
            mode,
            event
        });
    if (mImpl) {
        mImpl->startTimer(id, event, timeout, mode);
    }
    return id;
}

uint32_t EventLoop::startTimer(std::function<void(uint32_t)>&& event, uint64_t timeout, TimerMode mode)
{
    const auto ev = std::make_shared<TimerEvent>(std::move(event));
    const uint32_t id = startTimer(ev, timeout, mode);
    ev->setId(id);
    return id;
}

void EventLoop::stopTimer(uint32_t id)
{
    std::lock_guard lock(mMutex);
    auto it = mTimers.begin();
    const auto end = mTimers.end();
    while (it != end) {
        if (it->id == id) {
            mTimers.erase(it);
            mImpl->stopTimer(id);
            break;
        }
        ++it;
    }
}

void* EventLoop::handle() const
{
    return mImpl->handle();
}
