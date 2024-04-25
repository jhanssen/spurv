#pragma once

#include "EventLoop.h"
typedef struct uv_async_s uv_async_t;
typedef struct uv_timer_s uv_timer_t;

namespace spurv {

class EventLoopUv : public EventLoop
{
public:
    EventLoopUv();
    virtual ~EventLoopUv() override;

    virtual int32_t run() override;
    virtual void *handle() const override;
    virtual void post(std::unique_ptr<Event>&& event) override;
    using EventLoop::post;
    virtual uint32_t startTimer(const std::shared_ptr<EventLoop::Event>& event, uint64_t timeout, EventLoop::TimerMode mode) override;
    using EventLoop::startTimer;
    virtual void stopTimer(uint32_t id) override;
    virtual void stop(int32_t exitCode) override;

private:
    static void processPost(uv_async_t* handle);
    static void processTimer(uv_timer_t* handle);
    struct Data;
    Data *mData;
};

} // namespace spurv
