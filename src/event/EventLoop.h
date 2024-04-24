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

    virtual void run();
    virtual void stop();

    void post(std::function<void()>&& event);
    virtual void post(std::unique_ptr<Event>&& event);

    enum TimerMode
    {
        SingleShot,
        Repeat
    };
    uint32_t startTimer(std::function<void(uint32_t)>&& event, uint64_t timeout, TimerMode mode = TimerMode::SingleShot);
    virtual uint32_t startTimer(const std::shared_ptr<Event>& event, uint64_t timeout, TimerMode mode = TimerMode::SingleShot);
    virtual void stopTimer(uint32_t id);

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

    virtual void* handle() const;

private:
    void run_internal();
    void post_internal(std::unique_ptr<Event>&& event);
    void stop_internal();
    uint32_t startTimer_internal(const std::shared_ptr<Event>& event, uint64_t timeout, TimerMode mode);
    void stopTimer_internal(uint32_t id);
    bool processEvents();

private:
    std::mutex mMutex;

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

    // ### probably just hold two raw pointers here instead of a variant
    std::variant<EventLoopImplMain*, EventLoopImplUv*, std::nullopt_t> mImpl = std::nullopt;

protected:
    static EventLoop* sMainEventLoop;

private:
    thread_local static EventLoop* tEventLoop;

    friend class MainEventLoop;
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
