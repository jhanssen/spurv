#pragma once

#include "EventLoop.h"
#include "EventEmitter.h"

namespace spurv {

class MainEventLoop : public EventLoop
{
public:
    MainEventLoop();
    virtual ~MainEventLoop() override;

    virtual void run() override;
    virtual void stop() override;

    using EventLoop::post;
    virtual void post(std::unique_ptr<Event>&& event) override;

    using EventLoop::startTimer;
    virtual uint64_t startTimer(const std::shared_ptr<Event>& event, uint64_t timeout, TimerMode mode = TimerMode::SingleShot) override;
    virtual void stopTimer(uint64_t id) override;

    EventEmitter<void(uint32_t)>& onUnicode();

private:
    EventEmitter<void(uint32_t)> mOnUnicode;
};

inline EventEmitter<void(uint32_t)>& MainEventLoop::onUnicode()
{
    return mOnUnicode;
}

} // namespace spurv
