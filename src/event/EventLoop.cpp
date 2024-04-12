#include "EventLoop.h"
#include <uv.h>
#include <cassert>

using namespace spurv;

EventLoop* EventLoop::sMainEventLoop = nullptr;
thread_local EventLoop* EventLoop::tEventLoop = nullptr;

namespace spurv {
struct EventLoopImplUv
{
    EventLoopImplUv(EventLoop* l);
    ~EventLoopImplUv();

    static void processPost(uv_async_t* handle);
    static void processTimer(uv_timer_t* handle);

    EventLoop* loop;
    uv_loop_t uvloop;
    uv_async_t uvpost;
    struct UvTimer
    {
        uint64_t id;
        // holds the event shared_ptr alive for as long as
        // the uv_timer_t holds the raw pointer
        std::shared_ptr<EventLoop::Event> event;
        uv_timer_t timer;
    };
    std::vector<UvTimer> uvtimers;
};

EventLoopImplUv::EventLoopImplUv(EventLoop* l)
    : loop(l)
{
}

EventLoopImplUv::~EventLoopImplUv()
{
    uv_loop_close(&uvloop);
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

EventLoop::EventLoop()
{
}

EventLoop::~EventLoop()
{
    assert(std::holds_alternative<std::nullopt_t>(mImpl));
    if (sMainEventLoop == this) {
        sMainEventLoop = nullptr;
    }
    assert(tEventLoop == this);
    tEventLoop = nullptr;
}

void EventLoop::install()
{
    assert(tEventLoop == nullptr);
    tEventLoop = this;
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

void EventLoop::run()
{
    run_internal();
}

void EventLoop::stop()
{
    stop_internal();
}

void EventLoop::post(std::unique_ptr<Event>&& event)
{
    post_internal(std::move(event));
}

void EventLoop::post(std::function<void()>&& event)
{
    post(std::make_unique<FunctionEvent>(std::move(event)));
}

uint64_t EventLoop::startTimer(const std::shared_ptr<Event>& event, uint64_t timeout, TimerMode mode)
{
    return startTimer_internal(event, timeout, mode);
}

uint64_t EventLoop::startTimer(std::function<void()>&& event, uint64_t timeout, TimerMode mode)
{
    return startTimer(std::make_shared<FunctionEvent>(std::move(event)), timeout, mode);
}

void EventLoop::stopTimer(uint64_t id)
{
    stopTimer_internal(id);
}

void* EventLoop::handle() const
{
    auto uvImpl = std::get_if<EventLoopImplUv*>(&mImpl);
    if (uvImpl) {
        auto uvloop = &(*uvImpl)->uvloop;
        return uvloop;
    }
    return nullptr;
}

void EventLoop::post_internal(std::unique_ptr<Event>&& event)
{
    std::lock_guard lock(mMutex);
    mEvents.push_back(std::move(event));
    auto uvImpl = std::get_if<EventLoopImplUv*>(&mImpl);
    if (uvImpl) {
        uv_async_send(&(*uvImpl)->uvpost);
    }
}

uint64_t EventLoop::startTimer_internal(const std::shared_ptr<Event>& event, uint64_t timeout, TimerMode mode)
{
    auto id = ++mNextTimerId;
    std::lock_guard lock(mMutex);
    mTimers.push_back({
            id,
            timeout,
            mode,
            event
        });
    auto uvImpl = std::get_if<EventLoopImplUv*>(&mImpl);
    if (uvImpl) {
        auto uvloop = &(*uvImpl)->uvloop;
        auto& uvtimers = (*uvImpl)->uvtimers;
        uvtimers.push_back(EventLoopImplUv::UvTimer { id, event, {} });
        auto timer = &uvtimers.back().timer;
        uv_timer_init(uvloop, timer);
        timer->data = event.get();
        uv_timer_start(timer, EventLoopImplUv::processTimer, timeout, mode == TimerMode::SingleShot ? 0 : 1);
    }
    return id;
}

void EventLoop::stopTimer_internal(uint64_t id)
{
    std::lock_guard lock(mMutex);
    auto it = mTimers.begin();
    const auto end = mTimers.end();
    while (it != end) {
        if (it->id == id) {
            mTimers.erase(it);
            auto uvImpl = std::get_if<EventLoopImplUv*>(&mImpl);
            if (uvImpl) {
                auto& uvtimers = (*uvImpl)->uvtimers;
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
            break;
        }
        ++it;
    }
}

void EventLoop::stop_internal()
{
    std::lock_guard lock(mMutex);
    mStopped = true;

    if (std::holds_alternative<EventLoopImplUv*>(mImpl)) {
        auto uvImpl = std::get<EventLoopImplUv*>(mImpl);
        uv_stop(&uvImpl->uvloop);
        delete uvImpl;
        mImpl = std::nullopt;
    }
}

void EventLoop::run_internal()
{
    assert(std::holds_alternative<std::nullopt_t>(mImpl));
    EventLoopImplUv* uvImpl = new EventLoopImplUv(this);
    mImpl = uvImpl;
    uv_loop_init(&uvImpl->uvloop);
    uv_loop_set_data(&uvImpl->uvloop, this);
    uv_async_init(&uvImpl->uvloop, &uvImpl->uvpost, EventLoopImplUv::processPost);
    uvImpl->uvpost.data = this;

    // give events a chance to run
    processEvents();

    uv_run(&uvImpl->uvloop, UV_RUN_DEFAULT);
}
