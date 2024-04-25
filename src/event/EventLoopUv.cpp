#include "EventLoopUv.h"
#include <uv.h>
#include <cassert>

namespace spurv {

struct EventLoopUv::Data {
    std::mutex mutex;
    uv_loop_t uvloop;
    uv_async_t uvpost;
    struct Timer
    {
        uint32_t id;
        // holds the event shared_ptr alive for as long as
        // the uv_timer_t holds the raw pointer
        std::shared_ptr<EventLoop::Event> event;
        uv_timer_t timer;
    };
    std::vector<Timer> timers;
    int32_t exitCode = 0;
    uint32_t nextTimer = 0;
};

EventLoopUv::EventLoopUv()
    : mData(new Data)
{
}

EventLoopUv::~EventLoopUv()
{
    uv_loop_close(&mData->uvloop);
    delete mData;
}

int32_t EventLoopUv::run()
{
    assert(eventLoop() == this);
    uv_loop_init(&mData->uvloop);
    uv_loop_set_data(&mData->uvloop, this);
    uv_async_init(&mData->uvloop, &mData->uvpost, &processPost);
    mData->uvpost.data = this;

    // give events a chance to run
    processEvents();

    uv_run(&mData->uvloop, UV_RUN_DEFAULT);
    return mData->exitCode;
}

void EventLoopUv::stop(int32_t exitCode)
{
    std::lock_guard lock(mData->mutex);
    assert(eventLoop() == this || eventLoop() == nullptr);
    mData->exitCode = exitCode;
    uv_stop(&mData->uvloop);
}

void *EventLoopUv::handle() const
{
    return &mData->uvloop;
}

void EventLoopUv::post(std::unique_ptr<Event>&& event)
{
    EventLoop::post(std::move(event));
    uv_async_send(&mData->uvpost);
}

uint32_t EventLoopUv::startTimer(const std::shared_ptr<EventLoop::Event>& event, uint64_t timeout, EventLoop::TimerMode mode)
{
    std::lock_guard lock(mData->mutex);
    const uint32_t id = ++mData->nextTimer;
    mData->timers.push_back(Data::Timer { id, event, {} });
    auto timer = &mData->timers.back().timer;
    uv_timer_init(&mData->uvloop, timer);
    timer->data = event.get();
    uv_timer_start(timer, EventLoopUv::processTimer, timeout, mode == EventLoop::TimerMode::SingleShot ? 0 : 1);
    return id;
}

void EventLoopUv::stopTimer(uint32_t id)
{
    std::lock_guard lock(mData->mutex);

    auto uvit = mData->timers.begin();
    const auto uvend = mData->timers.end();
    while (uvit != uvend) {
        if (uvit->id == id) {
            uv_timer_stop(&uvit->timer);
            uv_close(reinterpret_cast<uv_handle_t*>(&uvit->timer), nullptr);
            mData->timers.erase(uvit);
            break;
        }
        ++uvit;
    }
}

void EventLoopUv::processPost(uv_async_t* handle)
{
    EventLoopUv* loop = static_cast<EventLoopUv*>(handle->data);
    loop->processEvents();
}

void EventLoopUv::processTimer(uv_timer_t* handle)
{
    EventLoop::Event* event = static_cast<EventLoop::Event*>(handle->data);
    executeEvent(event);
}

} // namespace spurv

