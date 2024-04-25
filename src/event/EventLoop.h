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
    };

    EventLoop();
    virtual ~EventLoop();

    void install();
    void uninstall();

    virtual int32_t run() = 0;
    virtual void stop(int32_t exitCode) = 0;

    void post(std::function<void()>&& event);
    virtual void post(std::unique_ptr<Event>&& event);

    enum class TimerMode
    {
        SingleShot,
        Repeat
    };
    uint32_t startTimer(std::function<void(uint32_t)>&& event, uint64_t timeout, TimerMode mode = TimerMode::SingleShot);
    virtual uint32_t startTimer(const std::shared_ptr<Event>& event, uint64_t timeout, TimerMode mode = TimerMode::SingleShot) = 0;
    virtual void stopTimer(uint32_t id) = 0;

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

    virtual void* handle() const = 0;

private:
    std::mutex mMutex;
    std::vector<std::unique_ptr<Event>> mEvents;

protected:
    void processEvents();
    static void executeEvent(Event *event);

    static EventLoop* sMainEventLoop;

private:
    thread_local static EventLoop* tEventLoop;
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
