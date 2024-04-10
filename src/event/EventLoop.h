#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <Signal.h>

namespace spurv {

class EventLoop
{
public:
    class Event
    {
    public:
        Event() = default;
        virtual ~Event() = default;

    protected:
        virtual void execute() = 0;

        friend class EventLoop;
    };

    EventLoop();
    ~EventLoop();

    void run();
    void stop();

    void post(std::unique_ptr<Event>&& event);
    void post(std::function<void()>&& event);

    Signal<void(uint32_t)>& onUnicode();

    static EventLoop* mainEventLoop();

private:
    void post_internal(std::unique_ptr<Event>&& event);
    void stop_internal();
    bool run_internal();

private:
    std::mutex mMutex;
    std::vector<std::unique_ptr<Event>> mEvents;
    Signal<void(uint32_t)> mOnUnicode;
    bool mStopped = false;

private:
    static EventLoop* sMainEventLoop;
};

inline Signal<void(uint32_t)>& EventLoop::onUnicode()
{
    return mOnUnicode;
}

inline EventLoop* EventLoop::mainEventLoop()
{
    return sMainEventLoop;
}

} // namespace spurv
