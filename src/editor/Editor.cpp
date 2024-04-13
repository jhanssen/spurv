#include "Editor.h"
#include <uv.h>

using namespace spurv;

namespace spurv {
struct EditorImpl
{
    uv_loop_t* loop = nullptr;
};
} // namespace spurv

std::unique_ptr<Editor> Editor::sInstance = {};

Editor::Editor()
    : mEventLoop(new EventLoop())
{
    mImpl = new EditorImpl();
    mThread = std::thread(&Editor::thread_internal, this);
}

Editor::~Editor()
{
    delete mImpl;
}

void Editor::thread_internal()
{
    // finalize the event loop
    mEventLoop->install();
    mEventLoop->post([this]() {
        {
            std::unique_lock lock(mMutex);
            mImpl->loop = static_cast<uv_loop_t*>(mEventLoop->handle());

            mInitialized = true;
        }

        EventLoop::mainEventLoop()->post([editor = this]() {
            editor->mOnReady.emit();
        });
    });
    mEventLoop->run();
}

void Editor::stop()
{
    {
        std::unique_lock lock(mMutex);
        assert(mInitialized);
    }
    mEventLoop->stop();
    mThread.join();
}

void Editor::initialize()
{
    assert(!sInstance);
    sInstance = std::unique_ptr<Editor>(new Editor());
}
