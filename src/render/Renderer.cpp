#include "Renderer.h"
#include "GenericPool.h"
#include "SemaphorePool.h"
#include <VulkanCommon.h>
#include <Window.h>
#include <Formatting.h>
#include <uv.h>
#include <VkBootstrap.h>
#include <fmt/core.h>
#include <vulkan/vulkan.h>
#include <vector>

using namespace spurv;

namespace spurv {
struct FenceInfo
{
    std::vector<std::function<void()>> callbacks;
    bool valid = false;
};

struct RendererImpl
{
    uv_idle_t idle;
    uv_loop_t* loop = nullptr;

    Renderer::Boxes boxes;

    vkb::Device vkbDevice = {};
    vkb::Swapchain vkbSwapchain = {};

    VkDevice device = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    std::vector<VkImage> images = {};
    std::vector<VkImageView> imageViews = {};

    std::vector<SemaphorePool> semaphores = {};
    uint32_t currentSwapchain = 0;
    uint32_t currentSwapchainImage = 0;

    GenericPool<VkCommandBuffer, 5> freeCommandBuffers = {};

    std::unordered_map<VkFence, FenceInfo> fenceInfos = {};
    GenericPool<VkFence, 5> freeFences = {};
    std::vector<std::function<void()>> frameCallbacks = {};

    void checkFence(VkFence fence);
    void checkFences();
    void runFenceCallbacks(FenceInfo& info);

    static void idleCallback(uv_idle_t* idle);
};

void RendererImpl::idleCallback(uv_idle_t* idle)
{
    Renderer* renderer = static_cast<Renderer*>(idle->data);
    renderer->render();
}

void RendererImpl::runFenceCallbacks(FenceInfo& info)
{
    for (auto& cb : info.callbacks) {
        cb();
    }
    info.callbacks.clear();
    info.valid = false;
}

void RendererImpl::checkFence(VkFence fence)
{
    const VkResult result = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    switch(result) {
    case VK_SUCCESS: {
        auto& info = fenceInfos[fence];
        runFenceCallbacks(info);
        break; }
    case VK_TIMEOUT:
        break;
    default:
        break;
    }
}

void RendererImpl::checkFences()
{
    for (auto& fence : fenceInfos) {
        if (!fence.second.valid) {
            continue;
        }
        const VkResult result = vkWaitForFences(device, 1, &fence.first, VK_TRUE, UINT64_MAX);
        switch(result) {
        case VK_SUCCESS:
            runFenceCallbacks(fence.second);
            break;
        case VK_TIMEOUT:
            break;
        default:
            break;
        }
    }
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
    auto maybeGraphicsQueueIndex = mImpl->vkbDevice.get_queue_index(vkb::QueueType::graphics);
    if (!maybeGraphicsQueueIndex) {
        fmt::print(stderr, "Unable to get vulkan graphics queue index: {}\n", maybeGraphicsQueueIndex.error().message());
        return;
    }

    auto maybePresentQueue = mImpl->vkbDevice.get_queue(vkb::QueueType::present);
    if (!maybePresentQueue) {
        fmt::print(stderr, "Unable to get vulkan present queue: {}\n", maybePresentQueue.error().message());
        return;
    }

    mImpl->graphicsQueue = maybeGraphicsQueue.value();
    mImpl->presentQueue = maybePresentQueue.value();

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = maybeGraphicsQueueIndex.value();
    VK_CHECK_SUCCESS(vkCreateCommandPool(mImpl->device, &poolInfo, nullptr, &mImpl->commandPool));

    mImpl->freeFences.initialize([impl = mImpl]() -> VkFence {
        VkFence fence;
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VK_CHECK_SUCCESS(vkCreateFence(impl->device, &fenceInfo, nullptr, &fence));
        return fence;
    }, [impl = mImpl](VkFence& fence) -> void {
        vkDestroyFence(impl->device, fence, nullptr);
    });

    mImpl->freeCommandBuffers.initialize([impl = mImpl]() -> VkCommandBuffer {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = impl->commandPool;
        allocInfo.commandBufferCount = 1;
        VkCommandBuffer commandBuffer;
        VK_CHECK_SUCCESS(vkAllocateCommandBuffers(impl->device, &allocInfo, &commandBuffer));
        return commandBuffer;
    }, [impl = mImpl](VkCommandBuffer& cmdbuffer) -> void {
        vkFreeCommandBuffers(impl->device, impl->commandPool, 1, &cmdbuffer);
    }, [](VkCommandBuffer& cmdbuffer) -> void {
        vkResetCommandBuffer(cmdbuffer, 0);
    });

    EventLoop::ConnectKey onResizeKey = {};

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

    mImpl->vkbSwapchain.destroy_image_views(mImpl->imageViews);
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
    mImpl->vkbSwapchain.destroy_image_views(mImpl->imageViews);
    vkb::destroy_swapchain(mImpl->vkbSwapchain);
    mImpl->vkbSwapchain = maybeSwapchain.value();
    auto maybeImages = mImpl->vkbSwapchain.get_images();
    if (!maybeImages) {
        fmt::print(stderr, "Unable to get swapchain images: {}\n", maybeImages.error().message());
        return false;
    }
    mImpl->images = maybeImages.value();
    auto maybeImageViews = mImpl->vkbSwapchain.get_image_views();
    if (!maybeImageViews) {
        fmt::print(stderr, "Unable to get swapchain image views: {}\n", maybeImageViews.error().message());
        return false;
    }
    mImpl->imageViews = maybeImageViews.value();
    mImpl->semaphores.resize(mImpl->imageViews.size());
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

    auto& semaphores = mImpl->semaphores;
    const auto currentSemaphore = mImpl->currentSwapchain;
    auto semaphore = semaphores[currentSemaphore].next();
    if(VkFence fence = semaphores[currentSemaphore].fence()) {
        //Log::info(TRACE_LOG, "[VulkanContext.cpp:%d]: WAITFENCE(%p) %p(%d)", __LINE__, fence, semaphore, mSwapCurrentSemaphore);
        mImpl->checkFence(fence);
        assert(!semaphores[currentSemaphore].fence());
    }
    VkResult acquired = vkAcquireNextImageKHR(mImpl->device, mImpl->vkbSwapchain.swapchain, UINT64_MAX, *semaphore, VK_NULL_HANDLE, &mImpl->currentSwapchainImage);
    switch (acquired) {
    case VK_SUCCESS:
        // everything good
        break;
    case VK_ERROR_OUT_OF_DATE_KHR:
        // recreate swapchain, try again later
        recreateSwapchain();
        return;
    default:
        // uh oh
        fmt::print(stderr, "Failed to acquire vulkan image [{}]\n", acquired);
        return;
    }

    auto cmdbuffer = *mImpl->freeCommandBuffers.get();
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK_SUCCESS(vkBeginCommandBuffer(cmdbuffer, &beginInfo));

    // if (!mImpl->boxes.empty()) {
    //     // fmt::print("render {}.\n", mImpl->boxes.size());
    // }

    VK_CHECK_SUCCESS(vkEndCommandBuffer(cmdbuffer));

    auto fence = *mImpl->freeFences.get();
    semaphores[currentSemaphore].setFence(fence);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    const static VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = semaphores[currentSemaphore].current();
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = semaphores[currentSemaphore].next();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdbuffer;
    const VkResult submitted = vkQueueSubmit(mImpl->graphicsQueue, 1, &submitInfo, fence);
    switch(submitted) {
    case VK_SUCCESS:
        mImpl->checkFences();
        break;
    default:
        fmt::print(stderr, "Failed to submit vulkan [{}]\n", submitted);
        break;
    }

    auto& fenceInfo = mImpl->fenceInfos[fence];
    fenceInfo.callbacks = std::move(mImpl->frameCallbacks);
    fenceInfo.valid = true;
    mImpl->frameCallbacks.clear();

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pImageIndices = &mImpl->currentSwapchainImage;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = semaphores[currentSemaphore].current();
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &mImpl->vkbSwapchain.swapchain;
    vkQueuePresentKHR(mImpl->presentQueue, &presentInfo);
    //Log::info(TRACE_LOG, "[VulkanContext.cpp:%d]: PRESENT(%d) %p(%d)", __LINE__, pending, presentInfo.pWaitSemaphores, mSwapCurrentSemaphore);
    semaphores[currentSemaphore].reset();
    mImpl->currentSwapchain = (currentSemaphore + 1) % semaphores.size();
    mImpl->currentSwapchainImage = UINT_MAX;
}

void Renderer::afterCurrentFrame(std::function<void()>&& func)
{
    mImpl->frameCallbacks.push_back(std::move(func));
}
