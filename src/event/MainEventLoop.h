#pragma once

#include "EventLoop.h"
#include "EventEmitter.h"

namespace spurv {

class MainEventLoop : public EventLoop
{
public:
    MainEventLoop();
    virtual ~MainEventLoop() override;

    virtual void *handle() const override;
    virtual void post(std::unique_ptr<Event>&& event) override;
    using EventLoop::post;
    virtual uint32_t startTimer(const std::shared_ptr<EventLoop::Event>& event, uint64_t timeout, EventLoop::TimerMode mode) override;
    using EventLoop::startTimer;
    virtual void stopTimer(uint32_t id) override;

    virtual int32_t run() override;
    virtual void stop(int32_t exitCode) override;

    EventEmitter<void(uint32_t)>& onUnicode();
    /**
     * glfwSetKeyCallback(GLFWwindow*, int key, int scancode, int action, int mods)
     */

    EventEmitter<void(int, int, int, int)>& onKey();

private:
    struct MainEventLoopData;
    MainEventLoopData *mData;
    EventEmitter<void(uint32_t)> mOnUnicode;
    EventEmitter<void(int, int, int, int)> mOnKey;
};

inline EventEmitter<void(uint32_t)>& MainEventLoop::onUnicode()
{
    return mOnUnicode;
}

inline EventEmitter<void(int, int, int, int)>& MainEventLoop::onKey()
{
    return mOnKey;
}
} // namespace spurv
