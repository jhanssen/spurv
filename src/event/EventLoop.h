#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <variant>
#include <vector>
#include <optional>
#include <cstdint>
#include <EventEmitter.h>

namespace spurv {

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
        friend struct EventLoopImplMain;
        friend struct EventLoopImplUv;
    };

    EventLoop();
    ~EventLoop();

    void run();
    void stop();

    void post(std::unique_ptr<Event>&& event);
    void post(std::function<void()>&& event);

    enum TimerMode
    {
        SingleShot,
        Repeat
    };
    uint64_t startTimer(const std::shared_ptr<Event>& event, uint64_t timeout, TimerMode mode = TimerMode::SingleShot);
    uint64_t startTimer(std::function<void()>&& event, uint64_t timeout, TimerMode mode = TimerMode::SingleShot);
    void stopTimer(uint64_t id);

    EventEmitter<void(uint32_t)>& onUnicode();

    bool isMainEventLoop() const;
    static EventLoop* mainEventLoop();

private:
    void run_internal();
    void post_internal(std::unique_ptr<Event>&& event);
    void stop_internal();
    uint64_t startTimer_internal(const std::shared_ptr<Event>& event, uint64_t timeout, TimerMode mode);
    void stopTimer_internal(uint64_t id);
    bool processEvents();

private:
    std::mutex mMutex;

    std::vector<std::unique_ptr<Event>> mEvents;

    struct Timer
    {
        uint64_t id;
        uint64_t timeout;
        TimerMode mode;
        std::shared_ptr<Event> event;
    };
    std::vector<Timer> mTimers;
    uint64_t mNextTimerId = 0;

    EventEmitter<void(uint32_t)> mOnUnicode;
    bool mStopped = false;

    // ### probably just hold two raw pointers here instead of a variant
    std::variant<EventLoopImplMain*, EventLoopImplUv*, std::nullopt_t> mImpl = std::nullopt;

private:
    static EventLoop* sMainEventLoop;

    friend struct EventLoopImplMain;
    friend struct EventLoopImplUv;
};

inline EventEmitter<void(uint32_t)>& EventLoop::onUnicode()
{
    return mOnUnicode;
}

inline EventLoop* EventLoop::mainEventLoop()
{
    return sMainEventLoop;
}

inline bool EventLoop::isMainEventLoop() const
{
    return sMainEventLoop == this;
}

} // namespace spurv
