#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <variant>
#include <vector>
#include <optional>
#include <cstdint>

namespace spurv {

class MainEventLoop;
struct EventLoopImplMain;
struct EventLoopImplUv;

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
        friend class MainEventLoop;
        friend struct EventLoopImplMain;
        friend struct EventLoopImplUv;
    };

    EventLoop();
    virtual ~EventLoop();

    void install();
    void uninstall();

    virtual int32_t run();
    void stop(int32_t exitCode);

    void post(std::function<void()>&& event);
    void post(std::unique_ptr<Event>&& event);

    enum class TimerMode
    {
        SingleShot,
        Repeat
    };
    uint32_t startTimer(std::function<void(uint32_t)>&& event, uint64_t timeout, TimerMode mode = TimerMode::SingleShot);
    uint32_t startTimer(const std::shared_ptr<Event>& event, uint64_t timeout, TimerMode mode = TimerMode::SingleShot);
    void stopTimer(uint32_t id);

    bool isMainEventLoop() const;
    static EventLoop* mainEventLoop();
    static EventLoop* eventLoop();

    enum class ConnectMode
    {
        Auto,
        Direct,
        Queued
    };
    using ConnectKey = uint32_t;

    void* handle() const;

private:
    struct EventLoopImpl
    {
        EventLoopImpl(EventLoop *loop)
            : eventLoop(loop)
        {}
        virtual ~EventLoopImpl() = default;
        virtual void *handle() = 0;
        virtual void post() = 0;
        virtual void startTimer(uint32_t /*id*/, const std::shared_ptr<Event>& /*event*/, uint64_t /*ms*/, TimerMode /*mode*/) = 0;
        virtual void stopTimer(uint32_t /*id*/) = 0;
        virtual void stop() = 0;

        EventLoop *const eventLoop;
    };
    std::vector<std::unique_ptr<Event>> mEvents;

    struct Timer
    {
        uint32_t id;
        uint64_t timeout;
        TimerMode mode;
        std::shared_ptr<Event> event;
    };
    std::vector<Timer> mTimers;
    uint32_t mNextTimerId = 0;

    bool mStopped = false;

protected:
    bool processEvents();
    std::mutex mMutex;
    std::unique_ptr<EventLoopImpl> mImpl;
    static EventLoop* sMainEventLoop;
    int32_t mExitCode = 0;

private:
    thread_local static EventLoop* tEventLoop;

    friend struct EventLoopImplMain;
    friend struct EventLoopImplUv;
};

inline EventLoop* EventLoop::mainEventLoop()
{
    return sMainEventLoop;
}

inline EventLoop* EventLoop::eventLoop()
{
    return tEventLoop;
}

inline bool EventLoop::isMainEventLoop() const
{
    return sMainEventLoop == this;
}

} // namespace spurv
