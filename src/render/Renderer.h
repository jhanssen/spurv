#pragma once

#include <EventLoop.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <memory>
#include <cassert>

namespace spurv {

struct RendererImpl;

class Renderer
{
public:
    ~Renderer();

    static void initialize();
    static void destroy();
    static Renderer* instance();
    static EventLoop* eventLoop();

private:
    Renderer();

    void waitForInitalize();
    void thread_internal();
    void render();
    void stop();

private:
    static std::unique_ptr<Renderer> sInstance;

    std::mutex mMutex;
    std::condition_variable mCond;
    std::thread mThread;
    std::unique_ptr<EventLoop> mEventLoop;
    bool mInitialized = false, mStopped = true;

    RendererImpl* mImpl;

    friend struct RendererImpl;
};

inline Renderer* Renderer::instance()
{
    assert(sInstance);
    return sInstance.get();
}

inline EventLoop* Renderer::eventLoop()
{
    assert(sInstance);
    assert(sInstance->mEventLoop);
    return sInstance->mEventLoop.get();
}

inline void Renderer::destroy()
{
    auto instance = sInstance.get();
    if (!instance)
        return;
    instance->stop();
    sInstance.reset();
}

} // namespace spurv
