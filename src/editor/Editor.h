#pragma once

#include <EventEmitter.h>
#include <EventLoop.h>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace spurv {

struct EditorImpl;

class Editor
{
public:
    ~Editor();

    static void initialize();
    static void destroy();
    static Editor* instance();
    static EventLoop* eventLoop();

    EventEmitter<void()>& onReady();

private:
    Editor();

    Editor(Editor&&) = delete;
    Editor(const Editor&) = delete;
    Editor& operator=(Editor&&) = delete;
    Editor& operator=(const Editor&) = delete;

    void thread_internal();
    void stop();

private:
    static std::unique_ptr<Editor> sInstance;

    std::mutex mMutex;
    std::condition_variable mCond;
    std::thread mThread;
    std::unique_ptr<EventLoop> mEventLoop;
    EventEmitter<void()> mOnReady;
    bool mInitialized = false;

    EditorImpl* mImpl;
};

inline Editor* Editor::instance()
{
    assert(sInstance);
    return sInstance.get();
}

inline EventLoop* Editor::eventLoop()
{
    assert(sInstance);
    assert(sInstance->mEventLoop);
    return sInstance->mEventLoop.get();
}

inline void Editor::destroy()
{
    auto instance = sInstance.get();
    if (!instance)
        return;
    instance->stop();
    sInstance.reset();
}

inline EventEmitter<void()>& Editor::onReady()
{
    return mOnReady;
}

} // namespace spurv
