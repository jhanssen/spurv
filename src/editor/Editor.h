#pragma once

#include "Container.h"
#include <Document.h>
#include <EventEmitter.h>
#include <EventLoop.h>
#include <Styleable.h>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>

namespace spurv {

struct EditorImpl;
class ScriptEngine;

class Editor : public Styleable
{
public:
    ~Editor();

    static void initialize(const std::filesystem::path &appPath);
    static void destroy();
    static Editor* instance();
    static EventLoop* eventLoop();

    bool isReady() const;
    EventEmitter<void()>& onReady();
    void load(const std::filesystem::path& path);

    virtual void setName(const std::string& name) override;

private:
    Editor(const std::filesystem::path &appPath);

    Editor(Editor&&) = delete;
    Editor(const Editor&) = delete;
    Editor& operator=(Editor&&) = delete;
    Editor& operator=(const Editor&) = delete;

    void thread_internal();
    void stop();

private:
    static std::unique_ptr<Editor> sInstance;

    mutable std::mutex mMutex;

    std::unique_ptr<ScriptEngine> mScriptEngine;
    std::condition_variable mCond;
    std::thread mThread;
    std::unique_ptr<EventLoop> mEventLoop;
    std::vector<std::shared_ptr<Document>> mDocuments;
    std::unique_ptr<Container> mContainer;
    std::array<EventLoop::ConnectKey, 2> mConnectKeys;
    std::string mName;
    View* mCurrentView = nullptr;
    EventEmitter<void()> mOnReady;
    EditorImpl* mImpl;

    bool mInitialized = false;

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

inline bool Editor::isReady() const
{
    std::scoped_lock lock(mMutex);
    return mInitialized;
}

} // namespace spurv
