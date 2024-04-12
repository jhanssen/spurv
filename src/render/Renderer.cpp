#include "Renderer.h"
#include <Window.h>
#include <uv.h>
#include <VkBootstrap.h>
#include <fmt/core.h>

using namespace spurv;

namespace spurv {
struct RendererImpl
{
    uv_idle_t idle;
    uv_loop_t* loop = nullptr;

    Renderer::Boxes boxes;

    vkb::Device vkbDevice = {};
    vkb::Swapchain vkbSwapchain = {};

    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;

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
    // initialize vulkan
    vkb::InstanceBuilder builder;
    auto maybeInstance = builder
        .set_app_name("spurv")
        .request_validation_layers()
        .use_default_debug_messenger()
        .build();
    if (!maybeInstance) {
        fmt::print(stderr, "Unable to create vulkan instance: {}\n", maybeInstance.error().message());
        return;
    }
    auto instance = maybeInstance.value();

    auto window = Window::mainWindow();
    if (window == nullptr) {
        fmt::print(stderr, "No window created\n");
        return;
    }

    auto surface = window->surface(instance);
    if (surface == VK_NULL_HANDLE) {
        fmt::print(stderr, "No vulkan surface\n");
        return;
    }

    vkb::PhysicalDeviceSelector selector { instance };
    auto maybePhysicalDevice = selector
        .set_surface(surface)
        .set_minimum_version(1, 2)
        .select();
    if (!maybePhysicalDevice) {
        fmt::print(stderr, "Unable to select vulkan physical device: {}\n", maybePhysicalDevice.error().message());
        return;
    }

    vkb::DeviceBuilder deviceBuilder { maybePhysicalDevice.value() };
    auto maybeDevice = deviceBuilder.build();
    if (!maybeDevice) {
        fmt::print(stderr, "Unable to create vulkan device: {}\n", maybeDevice.error().message());
        return;
    }

    mImpl->vkbDevice = maybeDevice.value();
    mImpl->device = mImpl->vkbDevice.device;

    auto maybeGraphicsQueue = mImpl->vkbDevice.get_queue(vkb::QueueType::graphics);
    if (!maybeGraphicsQueue) {
        fmt::print(stderr, "Unable to get vulkan graphics queue: {}\n", maybeGraphicsQueue.error().message());
        return;
    }

    EventLoop::ConnectKey onResizeKey = {};

    mImpl->graphicsQueue = maybeGraphicsQueue.value();

    // finalize the event loop
    mEventLoop->install();

    // there's no uv_loop_t handle until run has been called
    // so post a task here that will be invoked after the
    // loop has fully started
    mEventLoop->post([this, window, &onResizeKey]() {
        recreateSwapchain();

        std::unique_lock lock(mMutex);
        mImpl->loop = static_cast<uv_loop_t*>(mEventLoop->handle());
        uv_idle_init(mImpl->loop, &mImpl->idle);
        mImpl->idle.data = this;
        uv_idle_start(&mImpl->idle, RendererImpl::idleCallback);

        onResizeKey = window->onResize().connect([this](uint32_t width, uint32_t height) {
            fmt::print(stderr, "Window resized, recreating swapchain {}x{}\n", width, height);
            recreateSwapchain();
        });

        mInitialized = true;
        mCond.notify_all();
    });

    mEventLoop->run();

    window->onResize().disconnect(onResizeKey);

    vkb::destroy_swapchain(mImpl->vkbSwapchain);
}

bool Renderer::recreateSwapchain()
{
    vkb::SwapchainBuilder swapchainBuilder { mImpl->vkbDevice };
    auto maybeSwapchain = swapchainBuilder
        .set_old_swapchain(mImpl->vkbSwapchain)
        .build();
    if (!maybeSwapchain) {
        fmt::print(stderr, "Unable to create swapchain: {}\n", maybeSwapchain.error().message());
        mImpl->vkbSwapchain.swapchain = VK_NULL_HANDLE;
        return false;
    }
    vkb::destroy_swapchain(mImpl->vkbSwapchain);
    mImpl->vkbSwapchain = maybeSwapchain.value();
    return true;
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
    // ### should possibly ensure that mImpl->idle has been stopped at this point
    mEventLoop->stop();
    mThread.join();
}

void Renderer::setBoxes(Boxes&& boxes)
{
    mEventLoop->post([boxes = std::move(boxes), impl = mImpl]() {
        impl->boxes = std::move(boxes);
    });
}

void Renderer::render()
{
    if (mImpl->vkbSwapchain.swapchain == VK_NULL_HANDLE) {
        return;
    }
    if (!mImpl->boxes.empty()) {
        // fmt::print("render {}.\n", mImpl->boxes.size());
    }
}
