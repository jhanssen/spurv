#include <MainEventLoop.h>
#include <Logger.h>
#include <window/Window.h>
#include <window/glfw/GlfwUserData.h>
#include <volk.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <vector>
#include <cassert>

using namespace spurv;

namespace spurv {
struct EventLoopImplMain : public EventLoop::EventLoopImpl
{
    EventLoopImplMain(EventLoop* l);
    ~EventLoopImplMain();

    virtual void *handle() override;
    virtual void post() override;
    virtual void startTimer(uint32_t id, const std::shared_ptr<EventLoop::Event>& event, uint64_t timeout, EventLoop::TimerMode mode) override;
    virtual void stopTimer(uint32_t id) override;
    virtual void stop() override;

    struct Timer
    {
        uint64_t id;
        uint64_t started;
        uint64_t next;
        uint64_t timeout;
        std::shared_ptr<MainEventLoop::Event> event;
        bool repeats;
    };
    std::vector<Timer> timers;
};

EventLoopImplMain::EventLoopImplMain(EventLoop* loop)
    : EventLoopImpl(loop)
{
}

EventLoopImplMain::~EventLoopImplMain()
{
}

void *EventLoopImplMain::handle()
{
    return nullptr;
}

void EventLoopImplMain::post()
{
    assert(eventLoop->isMainEventLoop());
    glfwPostEmptyEvent();
}

void EventLoopImplMain::startTimer(uint32_t id, const std::shared_ptr<EventLoop::Event>& event, uint64_t timeout, EventLoop::TimerMode mode)
{
    assert(eventLoop->isMainEventLoop());
    const auto now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    std::lock_guard lock(eventLoop->mMutex);
    timers.push_back(EventLoopImplMain::Timer {
            id,
            now,
            now + timeout,
            timeout,
            event,
            mode == EventLoop::TimerMode::Repeat
        });
    // ###  optimize this by using std::lower_bound to insert the timer at the correct location
    std::sort(timers.begin(), timers.end(), [](EventLoopImplMain::Timer& t1, EventLoopImplMain::Timer& t2) {
        return t1.next < t2.next;
    });
    glfwPostEmptyEvent();
}

void EventLoopImplMain::stopTimer(uint32_t id)
{
    assert(eventLoop->isMainEventLoop());
    std::lock_guard lock(eventLoop->mMutex);
    auto it = timers.begin();
    const auto end = timers.end();
    while (it != end) {
        if (it->id == id) {
            timers.erase(it);
            break;
        }
        ++it;
    }
}

void EventLoopImplMain::stop()
{
    assert(eventLoop->isMainEventLoop());
    glfwPostEmptyEvent();
}
} // namespace spurv

MainEventLoop::MainEventLoop()
    : EventLoop()
{
    assert(sMainEventLoop == nullptr);
    sMainEventLoop = this;
    install();
}

MainEventLoop::~MainEventLoop()
{
    mImpl.reset();
}

int32_t MainEventLoop::run()
{
    assert(isMainEventLoop());

    assert(!mImpl);
    EventLoopImplMain* mainImpl = new EventLoopImplMain(this);
    mImpl.reset(mainImpl);

    auto mainWindow = Window::mainWindow()->glfwWindow();
    if (!mainWindow)
        return -1;

    GlfwUserData::set<1>(mainWindow, this);

    glfwSetCharCallback(mainWindow, [](GLFWwindow* win, unsigned int codepoint) {
        auto eventLoop = GlfwUserData::get<1, MainEventLoop>(win);
        eventLoop->mOnUnicode.emit(codepoint);
    });

    glfwSetKeyCallback(mainWindow, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
        auto eventLoop = GlfwUserData::get<1, MainEventLoop>(win);
        eventLoop->mOnKey.emit(key, scancode, action, mods);
    });

    for (;;) {
        if (!processEvents())
            break;
        std::unique_lock lock(mMutex);
        auto& timers = mainImpl->timers;
        if (timers.empty()) {
            lock.unlock();

            glfwWaitEvents();

            lock.lock();
        } else {
            // process timers
            auto now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

            // keep track of things to fire outside of the lock later
            // this ensures that we don't iterate over the event vector
            // while it could potentially be modified
            std::vector<std::shared_ptr<MainEventLoop::Event>> tofire;

            bool needsResort = false;
            auto it = timers.begin();
            while (it != timers.end()) {
                auto& timer = *it;
                if (timer.next <= now) {
                    tofire.push_back(timer.event);
                    if (timer.repeats) {
                        // reinsert and resort
                        do {
                            timer.next += timer.timeout;
                        } while (timer.next <= now);
                        if (timers.size() > 1) {
                            needsResort = true;
                        }
                    } else {
                        it = timers.erase(it);
                        continue;
                    }
                } else {
                    break;
                }
                ++it;
            }
            if (needsResort) {
                // ### possibly optimize this by removing the timer and reinserting it using std::lower_bound
                // I'm not 100% sure that would be better though
                std::sort(timers.begin(), timers.end(), [](EventLoopImplMain::Timer& t1, EventLoopImplMain::Timer& t2) {
                    return t1.next < t2.next;
                });
            }

            const uint64_t next = !timers.empty() ? timers.front().next : 0;
            lock.unlock();

            for (const auto& ev : tofire) {
                ev->execute();
            }

            if (next > 0) {
                now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
                if (next > now) {
                    glfwWaitEventsTimeout((next - now) / 1000.0);
                }
            } else {
                // next == 0 means timers were empty after
                // removing the SingleShot ones
                glfwWaitEvents();
            }

            lock.lock();
        }
    }
    return mExitCode;
}
