#pragma once

#include <EventEmitter.h>
#include <EventLoop.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <memory>
#include <optional>
#include <cstdint>
#include <cassert>
#include "Box.h"

namespace spurv {

struct RendererImpl;
class GenericPoolBase;

class Renderer
{
public:
    ~Renderer();

    static void initialize();
    static void destroy();
    static Renderer* instance();
    static EventLoop* eventLoop();

    using BoxRow = std::vector<Box>;
    using Boxes = std::vector<BoxRow>;

    void setBoxes(Boxes&& boxes);

    EventEmitter<void()>& onReady();

private:
    Renderer();

    void thread_internal();
    void render();
    void stop();

    bool recreateSwapchain();

    void afterCurrentFrame(std::function<void()>&& func);

private:
    static std::unique_ptr<Renderer> sInstance;

    std::mutex mMutex;
    std::condition_variable mCond;
    std::thread mThread;
    std::unique_ptr<EventLoop> mEventLoop;
    EventEmitter<void()> mOnReady;
    bool mInitialized = false; //, mStopped = true;

    RendererImpl* mImpl;

    friend struct RendererImpl;
    friend class GenericPoolBase;
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

inline EventEmitter<void()>& Renderer::onReady()
{
    return mOnReady;
}

} // namespace spurv
