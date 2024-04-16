#include "Renderer.h"
#include "GenericPool.h"
#include "GlyphAtlas.h"
#include "SemaphorePool.h"
#include <Logger.h>
#include <VulkanCommon.h>
#include <Window.h>
#include <uv.h>
#include <VkBootstrap.h>
#include <fmt/core.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>

using namespace spurv;

namespace spurv {
struct FenceInfo
{
    std::vector<std::function<void()>> callbacks;
    SemaphorePool* semaphore = nullptr;
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
    VkCommandPool graphicsCommandPool = VK_NULL_HANDLE;
    VkCommandPool transferCommandPool = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue transferQueue = VK_NULL_HANDLE;
    uint32_t graphicsFamily = 0;
    uint32_t transferFamily = 0;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
    std::vector<VkImage> images = {};
    std::vector<VkImageView> imageViews = {};

    GlyphTimeline transferTimeline = {};
    GlyphTimeline graphicsTimeline = {};

    std::vector<SemaphorePool> semaphores = {};
    uint32_t currentSwapchain = 0;
    uint32_t currentSwapchainImage = 0;

    uint32_t width = 0, height = 0;

    GenericPool<VkCommandBuffer, 5> freeGraphicsCommandBuffers = {};
    GenericPool<VkCommandBuffer, 5> freeTransferCommandBuffers = {};

    std::unordered_map<VkFence, FenceInfo> fenceInfos = {};
    GenericPool<VkFence, 5> freeFences = {};
    std::vector<std::function<void()>> frameCallbacks = {};

    bool suboptimal = false;

    void checkFence(VkFence fence);
    void checkFences();
    void runFenceCallbacks(FenceInfo& info);
    void addTextLines(uint32_t box, std::vector<TextLine>&& lines);

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
    if (info.semaphore) {
        info.semaphore->setFence(VK_NULL_HANDLE);
        info.semaphore = nullptr;
    }
}

void RendererImpl::checkFence(VkFence fence)
{
    VkResult result = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    switch(result) {
    case VK_SUCCESS: {
        auto& info = fenceInfos[fence];
        assert(info.valid);
        runFenceCallbacks(info);
        freeFences.makeAvailableNow(fence);
        break; }
    case VK_TIMEOUT:
        break;
    default:
        spdlog::critical("Unable to check fence: [{}]", result);
        break;
    }
}

void RendererImpl::checkFences()
{
    for (auto& fence : fenceInfos) {
        if (!fence.second.valid) {
            continue;
        }
        VkResult result = vkWaitForFences(device, 1, &fence.first, VK_TRUE, 0);
        switch(result) {
        case VK_SUCCESS:
            runFenceCallbacks(fence.second);
            freeFences.makeAvailableNow(fence.first);
            break;
        case VK_TIMEOUT:
            break;
        default:
            spdlog::critical("Unable to check fence: [{}]", result);
            break;
        }
    }
}

void RendererImpl::addTextLines(uint32_t box, std::vector<TextLine>&& lines)
{
    (void)box;
    spdlog::info("Got text lines {}", lines.size());
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
        spdlog::critical("Unable to create vulkan instance: {}", maybeInstance.error().message());
        return;
    }
    auto instance = maybeInstance.value();

    auto window = Window::mainWindow();
    if (window == nullptr) {
        spdlog::critical("No window created");
        return;
    }

    auto surface = window->surface(instance);
    if (surface == VK_NULL_HANDLE) {
        spdlog::critical("No vulkan surface");
        return;
    }

    vkb::PhysicalDeviceSelector selector { instance };
    auto maybePhysicalDevice = selector
        .set_surface(surface)
        .set_minimum_version(1, 2)
        .require_separate_transfer_queue()
        .require_present()
        .select();
    if (!maybePhysicalDevice) {
        spdlog::critical("Unable to select vulkan physical device: {}", maybePhysicalDevice.error().message());
        return;
    }

    vkb::DeviceBuilder deviceBuilder { maybePhysicalDevice.value() };
    auto maybeDevice = deviceBuilder.build();
    if (!maybeDevice) {
        spdlog::critical("Unable to create vulkan device: {}", maybeDevice.error().message());
        return;
    }

    mImpl->vkbDevice = maybeDevice.value();
    mImpl->device = mImpl->vkbDevice.device;

    auto maybeGraphicsQueue = mImpl->vkbDevice.get_queue(vkb::QueueType::graphics);
    if (!maybeGraphicsQueue) {
        spdlog::critical("Unable to get vulkan graphics queue: {}", maybeGraphicsQueue.error().message());
        return;
    }
    auto maybeGraphicsQueueIndex = mImpl->vkbDevice.get_queue_index(vkb::QueueType::graphics);
    if (!maybeGraphicsQueueIndex) {
        spdlog::critical("Unable to get vulkan graphics queue index: {}", maybeGraphicsQueueIndex.error().message());
        return;
    }

    auto maybeTransferQueue = mImpl->vkbDevice.get_queue(vkb::QueueType::transfer);
    if (!maybeTransferQueue) {
        spdlog::critical("Unable to get vulkan transfer queue: {}", maybeTransferQueue.error().message());
        return;
    }
    auto maybeTransferQueueIndex = mImpl->vkbDevice.get_queue_index(vkb::QueueType::transfer);
    if (!maybeTransferQueueIndex) {
        spdlog::critical("Unable to get vulkan transfer queue index: {}", maybeTransferQueueIndex.error().message());
        return;
    }

    auto maybePresentQueue = mImpl->vkbDevice.get_queue(vkb::QueueType::present);
    if (!maybePresentQueue) {
        spdlog::critical("Unable to get vulkan present queue: {}", maybePresentQueue.error().message());
        return;
    }

    mImpl->graphicsQueue = maybeGraphicsQueue.value();
    mImpl->graphicsFamily = maybeGraphicsQueueIndex.value();
    mImpl->transferQueue = maybeTransferQueue.value();
    mImpl->transferFamily = maybeTransferQueueIndex.value();
    mImpl->presentQueue = maybePresentQueue.value();

    // create timeline semaphores for glyph atlas transfers
    VkSemaphoreTypeCreateInfo timelineInfo = {};
    timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineInfo.initialValue = mImpl->transferTimeline.value;
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = &timelineInfo;
    VK_CHECK_SUCCESS(vkCreateSemaphore(mImpl->device, &semaphoreInfo, nullptr, &mImpl->transferTimeline.semaphore));

    timelineInfo.initialValue = mImpl->graphicsTimeline.value;
    VK_CHECK_SUCCESS(vkCreateSemaphore(mImpl->device, &semaphoreInfo, nullptr, &mImpl->graphicsTimeline.semaphore));

    VkCommandPoolCreateInfo graphicsPoolInfo = {};
    graphicsPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    graphicsPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    graphicsPoolInfo.queueFamilyIndex = maybeGraphicsQueueIndex.value();
    VK_CHECK_SUCCESS(vkCreateCommandPool(mImpl->device, &graphicsPoolInfo, nullptr, &mImpl->graphicsCommandPool));

    VkCommandPoolCreateInfo transferPoolInfo = {};
    transferPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    transferPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    transferPoolInfo.queueFamilyIndex = maybeTransferQueueIndex.value();
    VK_CHECK_SUCCESS(vkCreateCommandPool(mImpl->device, &transferPoolInfo, nullptr, &mImpl->transferCommandPool));

    // create a vma instance
    VmaAllocatorCreateInfo vmaInfo = {};
    vmaInfo.physicalDevice = maybePhysicalDevice.value();
    vmaInfo.device = mImpl->device;
    vmaInfo.instance = instance;
    vmaInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    VK_CHECK_SUCCESS(vmaCreateAllocator(&vmaInfo, &mImpl->allocator));

    mImpl->freeFences.initialize([impl = mImpl]() -> VkFence {
        VkFence fence;
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VK_CHECK_SUCCESS(vkCreateFence(impl->device, &fenceInfo, nullptr, &fence));
        return fence;
    }, [impl = mImpl](VkFence fence) -> void {
        vkDestroyFence(impl->device, fence, nullptr);
    }, [impl = mImpl](VkFence fence) -> void {
        vkResetFences(impl->device, 1, &fence);
    }, GenericPoolBase::AvailabilityMode::Manual);

    mImpl->freeGraphicsCommandBuffers.initialize([impl = mImpl]() -> VkCommandBuffer {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = impl->graphicsCommandPool;
        allocInfo.commandBufferCount = 1;
        VkCommandBuffer commandBuffer;
        VK_CHECK_SUCCESS(vkAllocateCommandBuffers(impl->device, &allocInfo, &commandBuffer));
        return commandBuffer;
    }, [impl = mImpl](VkCommandBuffer cmdbuffer) -> void {
        vkFreeCommandBuffers(impl->device, impl->graphicsCommandPool, 1, &cmdbuffer);
    }, [](VkCommandBuffer cmdbuffer) -> void {
        vkResetCommandBuffer(cmdbuffer, 0);
    });

    mImpl->freeTransferCommandBuffers.initialize([impl = mImpl]() -> VkCommandBuffer {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = impl->transferCommandPool;
        allocInfo.commandBufferCount = 1;
        VkCommandBuffer commandBuffer;
        VK_CHECK_SUCCESS(vkAllocateCommandBuffers(impl->device, &allocInfo, &commandBuffer));
        return commandBuffer;
    }, [impl = mImpl](VkCommandBuffer cmdbuffer) -> void {
        vkFreeCommandBuffers(impl->device, impl->transferCommandPool, 1, &cmdbuffer);
    }, [](VkCommandBuffer cmdbuffer) -> void {
        vkResetCommandBuffer(cmdbuffer, 0);
    }, GenericPoolBase::AvailabilityMode::Manual);

    EventLoop::ConnectKey onResizeKey = {};

    // finalize the event loop
    mEventLoop->install();

    // there's no uv_loop_t handle until run has been called
    // so post a task here that will be invoked after the
    // loop has fully started
    mEventLoop->post([this, window, &onResizeKey]() {
        const auto& rect = window->rect();
        mImpl->width = rect.width;
        mImpl->height = rect.height;
        recreateSwapchain();

        {
            std::unique_lock lock(mMutex);
            mImpl->loop = static_cast<uv_loop_t*>(mEventLoop->handle());
            uv_idle_init(mImpl->loop, &mImpl->idle);
            mImpl->idle.data = this;
            uv_idle_start(&mImpl->idle, RendererImpl::idleCallback);

            onResizeKey = window->onResize().connect([this](uint32_t width, uint32_t height) {
                spdlog::info("Window resized, recreating swapchain {}x{}", width, height);
                mImpl->width = width;
                mImpl->height = height;
                recreateSwapchain();
            });

            mInitialized = true;
        }

        window->eventLoop()->post([renderer = this]() {
            renderer->mOnReady.emit();
        });
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
        .set_desired_extent(mImpl->width, mImpl->height)
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_RELAXED_KHR)
        .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .build();
    if (!maybeSwapchain) {
        spdlog::critical("Unable to create swapchain: {}", maybeSwapchain.error().message());
        mImpl->vkbSwapchain.swapchain = VK_NULL_HANDLE;
        return false;
    }
    mImpl->vkbSwapchain.destroy_image_views(mImpl->imageViews);
    vkb::destroy_swapchain(mImpl->vkbSwapchain);
    mImpl->vkbSwapchain = maybeSwapchain.value();
    auto maybeImages = mImpl->vkbSwapchain.get_images();
    if (!maybeImages) {
        spdlog::critical("Unable to get swapchain images: {}", maybeImages.error().message());
        return false;
    }
    mImpl->images = maybeImages.value();
    auto maybeImageViews = mImpl->vkbSwapchain.get_image_views();
    if (!maybeImageViews) {
        spdlog::critical("Unable to get swapchain image views: {}", maybeImageViews.error().message());
        return false;
    }
    mImpl->imageViews = maybeImageViews.value();
    mImpl->semaphores.resize(mImpl->imageViews.size());
    for (auto& sem : mImpl->semaphores) {
        sem.setDevice(mImpl->device);
    }
    return true;
}

void Renderer::initialize()
{
    assert(!sInstance);
    sInstance = std::unique_ptr<Renderer>(new Renderer());
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

void Renderer::addTextLines(uint32_t box, std::vector<TextLine>&& lines)
{
    mEventLoop->post([box, lines = std::move(lines), impl = mImpl]() mutable {
        impl->addTextLines(box, std::move(lines));
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
    if (VkFence fence = semaphores[currentSemaphore].fence()) {
        mImpl->checkFence(fence);
        assert(semaphores[currentSemaphore].fence() == VK_NULL_HANDLE);
    }
    VkResult acquired = vkAcquireNextImageKHR(mImpl->device, mImpl->vkbSwapchain.swapchain, UINT64_MAX, semaphore, VK_NULL_HANDLE, &mImpl->currentSwapchainImage);
    switch (acquired) {
    case VK_SUCCESS:
        // everything good
        if (mImpl->suboptimal) {
            spdlog::warn("Vulkan acquire normal");
            mImpl->suboptimal = false;
        }
        break;
    case VK_SUBOPTIMAL_KHR:
        spdlog::warn("Vulkan acquire suboptimal");
        mImpl->suboptimal = true;
        [[ fallthrough ]];
    case VK_ERROR_OUT_OF_DATE_KHR:
        // recreate swapchain, try again later
        recreateSwapchain();
        return;
    default:
        // uh oh
        spdlog::critical("Failed to acquire vulkan image [{}]", acquired);
        return;
    }

    auto swapImage = mImpl->images[mImpl->currentSwapchainImage];

    auto cmdbufferHandle = mImpl->freeGraphicsCommandBuffers.get();
    assert(cmdbufferHandle.isValid());
    auto cmdbuffer = *cmdbufferHandle;
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK_SUCCESS(vkBeginCommandBuffer(cmdbuffer, &beginInfo));

    // if (!mImpl->boxes.empty()) {
    //     // spdlog::info("render {}.", mImpl->boxes.size());
    // }

    VkImageMemoryBarrier attachmentBarrier = {};
    attachmentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    attachmentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    attachmentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    attachmentBarrier.image = swapImage;
    attachmentBarrier.srcAccessMask = VK_ACCESS_NONE;
    attachmentBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    attachmentBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    attachmentBarrier.subresourceRange.baseMipLevel = 0;
    attachmentBarrier.subresourceRange.levelCount = 1;
    attachmentBarrier.subresourceRange.baseArrayLayer = 0;
    attachmentBarrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cmdbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, {}, 0, nullptr, 0, nullptr, 1, &attachmentBarrier);

    VkImageMemoryBarrier presentBarrier = {};
    presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    presentBarrier.image = swapImage;
    presentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    presentBarrier.dstAccessMask = VK_ACCESS_NONE;
    presentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    presentBarrier.subresourceRange.baseMipLevel = 0;
    presentBarrier.subresourceRange.levelCount = 1;
    presentBarrier.subresourceRange.baseArrayLayer = 0;
    presentBarrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cmdbuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, {}, 0, nullptr, 0, nullptr, 1, &presentBarrier);

    VK_CHECK_SUCCESS(vkEndCommandBuffer(cmdbuffer));

    auto fenceHandle = mImpl->freeFences.get();
    assert(fenceHandle.isValid());
    auto fence = *fenceHandle;
    auto& fenceInfo = mImpl->fenceInfos[fence];

    semaphores[currentSemaphore].setFence(fence);
    fenceInfo.semaphore = &semaphores[currentSemaphore];

    VkSemaphore semCurrent = semaphores[currentSemaphore].current();
    VkSemaphore semNext = semaphores[currentSemaphore].next();

    // wait for the graphics timeline semaphore
    uint64_t timelineSemValues[] = {
        0, // ignored
        mImpl->graphicsTimeline.value
    };
    VkSemaphore waitSems[] = {
        semCurrent,
        mImpl->graphicsTimeline.semaphore
    };

    VkTimelineSemaphoreSubmitInfo timelineInfo = {};
    timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timelineInfo.waitSemaphoreValueCount = 2;
    timelineInfo.pWaitSemaphoreValues = timelineSemValues;
    timelineInfo.signalSemaphoreValueCount = 0;
    timelineInfo.pSignalSemaphoreValues = nullptr;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = &timelineInfo;
    const static VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.waitSemaphoreCount = 2;
    submitInfo.pWaitSemaphores = waitSems;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &semNext;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdbuffer;
    const VkResult submitted = vkQueueSubmit(mImpl->graphicsQueue, 1, &submitInfo, fence);
    switch(submitted) {
    case VK_SUCCESS:
        mImpl->checkFences();
        break;
    default:
        spdlog::critical("Failed to submit vulkan [{}]", submitted);
        break;
    }

    {
        std::unique_lock lock(mMutex);
        fenceInfo.callbacks = std::move(mImpl->frameCallbacks);
        fenceInfo.valid = true;
        mImpl->frameCallbacks.clear();
    }

    // fmt::print("render\n");

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pImageIndices = &mImpl->currentSwapchainImage;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &semNext;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &mImpl->vkbSwapchain.swapchain;
    vkQueuePresentKHR(mImpl->presentQueue, &presentInfo);
    semaphores[currentSemaphore].reset();
    mImpl->currentSwapchain = (currentSemaphore + 1) % semaphores.size();
    mImpl->currentSwapchainImage = UINT_MAX;
}

void Renderer::afterCurrentFrame(std::function<void()>&& func)
{
    std::unique_lock lock(mMutex);
    mImpl->frameCallbacks.push_back(std::move(func));
}

void Renderer::glyphsCreated(GlyphsCreated&& created)
{
    // free transfer command buffer
    mImpl->freeTransferCommandBuffers.makeAvailableNow(created.commandBuffer);

    // finalize transferring the ownership of the image
    auto cmdbufferHandle = mImpl->freeGraphicsCommandBuffers.get();
    assert(cmdbufferHandle.isValid());
    auto cmdbuffer = *cmdbufferHandle;
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK_SUCCESS(vkBeginCommandBuffer(cmdbuffer, &beginInfo));

    // transition image to copy-dst
    VkImageMemoryBarrier imgMemBarrier = {};
    imgMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imgMemBarrier.srcQueueFamilyIndex = mImpl->transferFamily;
    imgMemBarrier.dstQueueFamilyIndex = mImpl->graphicsFamily;
    imgMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imgMemBarrier.subresourceRange.baseMipLevel = 0;
    imgMemBarrier.subresourceRange.levelCount = 1;
    imgMemBarrier.subresourceRange.baseArrayLayer = 0;
    imgMemBarrier.subresourceRange.layerCount = 1;
    imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imgMemBarrier.image = created.image;
    imgMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imgMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        cmdbuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imgMemBarrier);

    VK_CHECK_SUCCESS(vkEndCommandBuffer(cmdbuffer));

    // submit and signal the graphics timeline
    uint64_t semWait = created.wait;
    uint64_t semSignal = ++mImpl->graphicsTimeline.value;
    VkTimelineSemaphoreSubmitInfo timelineInfo = {};
    timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timelineInfo.waitSemaphoreValueCount = 1;
    timelineInfo.pWaitSemaphoreValues = &semWait;
    timelineInfo.signalSemaphoreValueCount = 1;
    timelineInfo.pSignalSemaphoreValues = &semSignal;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = &timelineInfo;
    const static VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &mImpl->transferTimeline.semaphore;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &mImpl->graphicsTimeline.semaphore;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdbuffer;
    VK_CHECK_SUCCESS(vkQueueSubmit(mImpl->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
}
