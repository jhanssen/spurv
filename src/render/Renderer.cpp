#include "Renderer.h"
#include <uv.h>
#include <fmt/core.h>

using namespace spurv;

namespace spurv {
struct RendererImpl
{
    uv_idle_t idle;
    uv_loop_t* loop;

    static void idleCallback(uv_idle_t* idle);
};

void RendererImpl::idleCallback(uv_idle_t* idle)
{
    Renderer* renderer = static_cast<Renderer*>(idle->data);
    renderer->render();
}
} // namespace spurv

std::unique_ptr<Renderer> Renderer::sInstance = {};

Renderer::Renderer()
    : mEventLoop(new EventLoop())
{
    mImpl = new RendererImpl();
    mThread = std::thread(&Renderer::thread_internal, this);
}

Renderer::~Renderer()
{
    delete mImpl;
}

void Renderer::thread_internal()
{
    mEventLoop->install();
    // there's no uv_loop_t handle until run has been called
    // so post a task here that will be invoked after the
    // loop has fully started
    mEventLoop->post([this]() {
        std::unique_lock lock(mMutex);
        mImpl->loop = static_cast<uv_loop_t*>(mEventLoop->handle());
        uv_idle_init(mImpl->loop, &mImpl->idle);
        mImpl->idle.data = this;
        uv_idle_start(&mImpl->idle, RendererImpl::idleCallback);
        mInitialized = true;
        mCond.notify_all();
    });
    mEventLoop->run();
}

void Renderer::waitForInitalize()
{
    std::unique_lock lock(mMutex);
    while (!mInitialized) {
        mCond.wait(lock);
    }
}

void Renderer::initialize()
{
    assert(!sInstance);
    sInstance = std::unique_ptr<Renderer>(new Renderer());
    sInstance->waitForInitalize();
}

void Renderer::stop()
{
    {
        std::unique_lock lock(mMutex);
        assert(mInitialized);
    }
    mEventLoop->stop();
    mThread.join();
}

void Renderer::render()
{
    // fmt::print("render.\n");
}
