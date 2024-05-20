#pragma once

#include "Container.h"
#include <Document.h>
#include <EventEmitter.h>
#include <EventLoop.h>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>

namespace spurv {

struct EditorImpl;
class ScriptEngine;

class Editor
{
public:
    ~Editor();

    static void initialize(const std::filesystem::path &appPath, int argc, char **argv, char **envp);
    static void destroy();
    static Editor* instance();
    static EventLoop* eventLoop();

    bool isReady() const;
    EventEmitter<void()>& onReady();
    void load(const std::filesystem::path& path);

    void setName(const std::string& name);

    using StylesheetMode = Document::StylesheetMode;

    void setStylesheet(const std::string& qss, StylesheetMode mode = StylesheetMode::Replace);
    void setStylesheet(const qss::Document& qss, StylesheetMode mode = StylesheetMode::Replace);
    const qss::Document& stylesheet() const;

    int argc() const;
    char **argv() const;
    char **envp() const;

private:
    Editor(const std::filesystem::path &appPath, int argc, char **argv, char **envp);

    Editor(Editor&&) = delete;
    Editor(const Editor&) = delete;
    Editor& operator=(Editor&&) = delete;
    Editor& operator=(const Editor&) = delete;

    void thread_internal();
    void stop();

    void relayout();

private:
    static std::unique_ptr<Editor> sInstance;

    mutable std::mutex mMutex;

    std::unique_ptr<ScriptEngine> mScriptEngine;
    std::condition_variable mCond;
    std::thread mThread;
    std::unique_ptr<EventLoop> mEventLoop;
    std::vector<std::shared_ptr<Document>> mDocuments;
    std::unique_ptr<Container> mContainer;
    std::array<EventLoop::ConnectKey, 3> mConnectKeys;
    std::string mName;
    qss::Document mQss;
    uint32_t mWidth = 0, mHeight = 0;
    std::shared_ptr<View> mCurrentView = {};
    EventEmitter<void()> mOnReady;
    EditorImpl* mImpl;
    const int mArgc;
    char **const mArgv;
    char **const mEnvp;

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

inline const qss::Document& Editor::stylesheet() const
{
    return mQss;
}

} // namespace spurv
