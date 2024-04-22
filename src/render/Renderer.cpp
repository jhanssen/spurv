#include "Renderer.h"
#include "GenericPool.h"
#include "GlyphAtlas.h"
#include "GraphicsPipeline.h"
#include "TextVBO.h"
#include "SemaphorePool.h"
#include <Logger.h>
#include <VulkanCommon.h>
#include <Window.h>
#include <uv.h>
#include <VkBootstrap.h>
#include <fmt/core.h>
#include <vk_mem_alloc.h>
#include <unordered_set>
#include <vector>
#include <cassert>

using namespace spurv;

#if defined(__APPLE__) && defined(_LIBCPP_VERSION)
# if _LIBCPP_VERSION < 170006
// wonderful
namespace std
{
    template<>
    struct hash<std::filesystem::path> {
        std::size_t operator()(const std::filesystem::path& v) const
        {
            return std::hash<std::string>()(v.string());
        }
    };
};
# endif
#endif

namespace spurv_vk {
PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
}

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
    std::vector<VkFramebuffer> swapchainFramebuffers = {};
    VkRenderPass swapchainRenderPass = VK_NULL_HANDLE;

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
    uint32_t scaledWidth = 0, scaledHeight = 0;

    GenericPool<VkCommandBuffer, 5> freeGraphicsCommandBuffers = {};
    GenericPool<VkCommandBuffer, 32> freeTransferCommandBuffers = {};

    std::unordered_map<VkFence, FenceInfo> fenceInfos = {};
    GenericPool<VkFence, 5> freeFences = {};
    std::vector<std::function<void()>> afterFrameCallbacks = {};
    uint64_t highestAfterTransfer = 0;
    std::vector<std::pair<uint64_t, std::function<void()>>> afterTransfers = {};
    std::vector<std::function<void(VkCommandBuffer cmdbuffer)>> inFrameCallbacks = {};

    std::unordered_map<std::filesystem::path, GlyphAtlas> glyphAtlases = {};
    std::vector<std::vector<TextLine>> textLines = {};
    std::vector<std::vector<TextVBO>> textVBOs = {};
    VkSampler textSampler = VK_NULL_HANDLE;
    VkBuffer geomUniformBuffer = VK_NULL_HANDLE;
    VmaAllocation geomUniformBufferAllocation = VK_NULL_HANDLE;
    VkBuffer colorUniformBuffer = VK_NULL_HANDLE;
    VmaAllocation colorUniformBufferAllocation = VK_NULL_HANDLE;

    VkDescriptorSetLayout textUniformLayout = VK_NULL_HANDLE;
    VkPipelineLayout textPipelineLayout = VK_NULL_HANDLE;
    VkPipeline textPipeline = VK_NULL_HANDLE;

    SizeF contentScale = { 1.f, 1.f };

    bool suboptimal = false;

    void checkFence(VkFence fence);
    void checkFences();
    void runFenceCallbacks(FenceInfo& info);
    void addTextLines(uint32_t box, std::vector<TextLine>&& lines);
    void generateVBOs(VkCommandBuffer cmdbuffer);
    void recreateUniformBuffers();
    GlyphAtlas& atlasFor(const Font& font);

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

GlyphAtlas& RendererImpl::atlasFor(const Font& font)
{
    auto it = glyphAtlases.find(font.file());
    if (it != glyphAtlases.end()) {
        return it->second;
    }
    spdlog::info("creating glyph atlas for {}", font.file());
    auto& atlas = glyphAtlases[font.file()];
    atlas.setVulkanInfo({
            device,
            {
                graphicsQueue,
                graphicsFamily
            },
            {
                transferQueue,
                transferFamily
            },
            allocator
        });
    atlas.setFontFile(font.file());
    return atlas;
}

void RendererImpl::addTextLines(uint32_t box, std::vector<TextLine>&& lines)
{
    (void)box;
    spdlog::info("Got text lines {}", lines.size());

    GlyphAtlas* currentAtlas = nullptr;
    std::unordered_set<hb_codepoint_t> missing = {};

    for (const auto& line : lines) {
        auto& atlas = atlasFor(line.font);
        if ((!missing.empty() && currentAtlas != nullptr && currentAtlas != &atlas) || missing.size() >= 500) {
            auto cmdbuffer = freeTransferCommandBuffers.get();

            spdlog::info("generating glyphs {}", missing.size());
            currentAtlas->generate(std::move(missing), transferTimeline, *cmdbuffer);
            missing.clear();
        }
        currentAtlas = &atlas;

        uint32_t glyph_count;
        hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(line.buffer, &glyph_count);

        for (uint32_t i = 0; i < glyph_count; i++) {
            hb_codepoint_t glyphid = glyph_info[i].codepoint;

            auto glyphInfo = atlas.glyphBox(glyphid);
            if (glyphInfo == nullptr) {
                missing.insert(glyphid);
            }
        }
    }
    if (!missing.empty()) {
        assert(currentAtlas != nullptr);
        auto cmdbuffer = freeTransferCommandBuffers.get();

        spdlog::info("generating glyphs {}", missing.size());
        currentAtlas->generate(std::move(missing), transferTimeline, *cmdbuffer);
    }

    if (box >= textLines.size()) {
        textLines.resize(box + 1);
    }
    textLines[box] = std::move(lines);
    textVBOs.clear();
}

void RendererImpl::generateVBOs(VkCommandBuffer cmdbuffer)
{
    uint32_t missing = 0, generated = 0;

    spdlog::info("generating...");

    for (std::size_t box = 0; box < textLines.size(); ++box) {
        const auto& lines = textLines[box];
        if (lines.empty()) {
            continue;
        }
        if (box >= textVBOs.size()) {
            textVBOs.resize(box + 1);
        }

        float linePos = 0;
        auto& vbos = textVBOs[box];
        for (const auto& line : lines) {
            vbos.push_back(TextVBO());
            auto& vbo = vbos.back();
            auto& atlas = atlasFor(line.font);
            const auto fontSize = line.font.size();

            hb_font_extents_t fontExtents;
            hb_font_get_h_extents(line.font.font(), &fontExtents);
            const float lineHeight = ceilf(((fontExtents.ascender + fontExtents.descender + fontExtents.line_gap) / 64.f) + (fontSize / 4.f));
            const float baseLine = ceilf(fontExtents.ascender / 64.f);
            const float x_tracking = floorf(fontSize / 10.f);

            VkImageView view = VK_NULL_HANDLE;
            uint32_t glyph_count;
            float cursor_x = 0.f;
            hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(line.buffer, &glyph_count);
            for (uint32_t i = 0; i < glyph_count; i++) {
                hb_codepoint_t glyphid = glyph_info[i].codepoint;
                auto glyphInfo = atlas.glyphBox(glyphid);
                if (glyphInfo == nullptr || glyphInfo->image == VK_NULL_HANDLE) {
                    ++missing;
                    continue;
                }
                if (view == VK_NULL_HANDLE) {
                    view = glyphInfo->view;
                    vbo.setView(view);
                }
                assert(view == glyphInfo->view);

                const float x_left = glyphInfo->box.bounds.l * fontSize;
                const float x_right = glyphInfo->box.bounds.r * fontSize;
                const float x_advance = glyphInfo->box.advance * fontSize;
                const float y_bottom = glyphInfo->box.bounds.b * fontSize;
                const float y_top = glyphInfo->box.bounds.t * fontSize;

                vbo.add(
                    {
                        floorf(cursor_x) + floorf(x_left),
                        floorf(-y_bottom) + baseLine + linePos,
                        ceilf(x_right) - floorf(x_left) + 1.f,
                        floorf(y_bottom) - floorf(y_top) - 1.f
                    }, glyphInfo->box);

                cursor_x += x_advance + x_tracking;

                ++generated;
            }

            vbo.generate(allocator, cmdbuffer);

            // spdlog::info("generated so far {} {}", generated, vbo.size());
            linePos += lineHeight;
        }
    }

    spdlog::info("textvbos {} {}", generated, missing);
}

void RendererImpl::recreateUniformBuffers()
{
    // delete old buffers if they exist
    if (geomUniformBuffer != VK_NULL_HANDLE || colorUniformBuffer != VK_NULL_HANDLE) {
        Renderer::instance()->afterCurrentFrame([
            impl = this,
            geomBuffer = geomUniformBuffer,
            geomAlloc = geomUniformBufferAllocation,
            colorBuffer = colorUniformBuffer,
            colorAlloc = colorUniformBufferAllocation
            ]() -> void {
            if (geomBuffer != VK_NULL_HANDLE) {
                assert(geomAlloc != VK_NULL_HANDLE);
                vmaDestroyBuffer(impl->allocator, geomBuffer, geomAlloc);
            }
            if (colorBuffer != VK_NULL_HANDLE) {
                assert(colorAlloc != VK_NULL_HANDLE);
                vmaDestroyBuffer(impl->allocator, colorBuffer, colorAlloc);
            }
        });
        geomUniformBuffer = VK_NULL_HANDLE;
        geomUniformBufferAllocation = VK_NULL_HANDLE;
        colorUniformBuffer = VK_NULL_HANDLE;
        colorUniformBufferAllocation = VK_NULL_HANDLE;
    }

    inFrameCallbacks.push_back([impl = this](VkCommandBuffer cmdbuffer) -> void {
        // create uniform buffers for geometry and color

        // create a vulkan staging buffers
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = 4 * sizeof(float);
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo bufferAllocationInfo = {};
        bufferAllocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VkBuffer geomStagingBuffer = VK_NULL_HANDLE;
        VmaAllocation geomStagingBufferAllocation = VK_NULL_HANDLE;
        VK_CHECK_SUCCESS(vmaCreateBuffer(impl->allocator, &bufferInfo, &bufferAllocationInfo, &geomStagingBuffer, &geomStagingBufferAllocation, nullptr));

        // copy data to buffer
        void* data;
        VK_CHECK_SUCCESS(vmaMapMemory(impl->allocator, geomStagingBufferAllocation, &data));
        const float geomData[] = {
            0.f, 0.f,
            static_cast<float>(impl->width),
            static_cast<float>(impl->height)
        };
        ::memcpy(data, geomData, 4 * sizeof(float));
        vmaUnmapMemory(impl->allocator, geomStagingBufferAllocation);

        // create the geom ubo
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK_SUCCESS(vmaCreateBuffer(impl->allocator, &bufferInfo, &bufferAllocationInfo, &impl->geomUniformBuffer, &impl->geomUniformBufferAllocation, nullptr));

        // copy staging buffer to ubo
        VkBufferCopy bufferCopy = {};
        bufferCopy.size = 4 * sizeof(float);
        vkCmdCopyBuffer(cmdbuffer, geomStagingBuffer, impl->geomUniformBuffer, 1, &bufferCopy);

        // insert a memory barrier to ensure that the buffer copy has completed before it's used as a uniform buffer
        VkBufferMemoryBarrier memoryBarrier = {};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        memoryBarrier.size = VK_WHOLE_SIZE;
        memoryBarrier.buffer = impl->geomUniformBuffer;
        memoryBarrier.offset = 0;
        memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
        vkCmdPipelineBarrier(cmdbuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                             0,
                             0, nullptr,
                             1, &memoryBarrier,
                             0, nullptr);

        bufferInfo.size = 5 * sizeof(float);
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferAllocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VkBuffer colorStagingBuffer = VK_NULL_HANDLE;
        VmaAllocation colorStagingBufferAllocation = VK_NULL_HANDLE;
        VK_CHECK_SUCCESS(vmaCreateBuffer(impl->allocator, &bufferInfo, &bufferAllocationInfo, &colorStagingBuffer, &colorStagingBufferAllocation, nullptr));

        // copy data to buffer
        VK_CHECK_SUCCESS(vmaMapMemory(impl->allocator, colorStagingBufferAllocation, &data));
        const float colorData[] = {
            1.0f,
            1.0f,
            1.0f,
            1.0f,
            4.0f
        };
        ::memcpy(data, colorData, 5 * sizeof(float));
        vmaUnmapMemory(impl->allocator, colorStagingBufferAllocation);

        // create the color ubo
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK_SUCCESS(vmaCreateBuffer(impl->allocator, &bufferInfo, &bufferAllocationInfo, &impl->colorUniformBuffer, &impl->colorUniformBufferAllocation, nullptr));

        // copy staging buffer to ubo
        bufferCopy.size = 5 * sizeof(float);
        vkCmdCopyBuffer(cmdbuffer, colorStagingBuffer, impl->colorUniformBuffer, 1, &bufferCopy);

        // insert a memory barrier to ensure that the buffer copy has completed before it's used as a uniform buffer
        memoryBarrier.buffer = impl->colorUniformBuffer;
        vkCmdPipelineBarrier(cmdbuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0, nullptr,
                             1, &memoryBarrier,
                             0, nullptr);

        Renderer::instance()->afterCurrentFrame([
            allocator = impl->allocator,
            geomStagingBuffer, geomStagingBufferAllocation,
            colorStagingBuffer, colorStagingBufferAllocation
            ]() -> void {
            vmaDestroyBuffer(allocator, geomStagingBuffer, geomStagingBufferAllocation);
            vmaDestroyBuffer(allocator, colorStagingBuffer, colorStagingBufferAllocation);

            spdlog::info("Destroyed uniform staging buffers");
        });

        spdlog::info("Created uniform buffers");
    });
}

} // namespace spurv

std::unique_ptr<Renderer> Renderer::sInstance = {};

inline VkBool32 VKAPI_CALL vulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                               VkDebugUtilsMessageTypeFlagsEXT messageType,
                                               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                               void*) {
    auto ms = vkb::to_string_message_severity(messageSeverity);
    auto mt = vkb::to_string_message_type(messageType);
    switch (messageSeverity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        spdlog::debug("[{}: {}\n{}", ms, mt, pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        spdlog::error("[{}: {}\n{}", ms, mt, pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        spdlog::warn("[{}: {}\n{}", ms, mt, pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        spdlog::info("[{}: {}\n{}", ms, mt, pCallbackData->pMessage);
        break;
    default:
        // should never happen
        abort();
    }

    return VK_FALSE; // Applications must return false here
}

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

static inline vkb::Result<VkQueue> get_transfer_queue(vkb::Device& device, bool hasSeparateTransfer)
{
    if (hasSeparateTransfer) {
        return device.get_queue(vkb::QueueType::transfer);
    }
    for (std::size_t idx = 0; idx < device.queue_families.size(); ++idx) {
        auto& family = device.queue_families[idx];
        assert(family.queueFlags & VK_QUEUE_GRAPHICS_BIT);
        if (idx > 0 && (family.queueFlags & VK_QUEUE_TRANSFER_BIT)) {
            VkQueue out = VK_NULL_HANDLE;
            vkGetDeviceQueue(device.device, idx, 0, &out);
            if (out != VK_NULL_HANDLE) {
                return vkb::Result<VkQueue>(out);
            }
        }
    }
    return vkb::Result<VkQueue>(
        vkb::make_error_code(vkb::QueueError::transfer_unavailable)
        );
}

static inline vkb::Result<uint32_t> get_transfer_queue_index(vkb::Device& device, bool hasSeparateTransfer)
{
    if (hasSeparateTransfer) {
        return device.get_queue_index(vkb::QueueType::transfer);
    }
    for (std::size_t idx = 0; idx < device.queue_families.size(); ++idx) {
        auto& family = device.queue_families[idx];
        assert(family.queueFlags & VK_QUEUE_GRAPHICS_BIT);
        if (idx > 0 && (family.queueFlags & VK_QUEUE_TRANSFER_BIT)) {
            return vkb::Result<uint32_t>(static_cast<uint32_t>(idx));
        }
    }
    return vkb::Result<uint32_t>(
        vkb::make_error_code(vkb::QueueError::transfer_unavailable)
        );
}

void Renderer::thread_internal()
{
    if (volkInitialize() != VK_SUCCESS) {
        spdlog::critical("Failed to initialize volk/vulkan. You may need to download the Vulkan SDK and source the setup-env.sh script");
        spdlog::critical("- Remember to bundle the vulkan/moltenvk libraries/icds when making a mac bundle");
        abort();
    }

    // initialize vulkan
    vkb::InstanceBuilder builder;
    auto maybeInstance = builder
        .set_app_name("spurv")
        .require_api_version(1, 2, 0)
        .request_validation_layers()
        .set_debug_callback(vulkanDebugCallback)
        .build();
    if (!maybeInstance) {
        spdlog::critical("Unable to create vulkan instance: {}", maybeInstance.error().message());
        return;
    }
    auto instance = maybeInstance.value();
    volkLoadInstance(instance);

    auto window = Window::mainWindow();
    if (window == nullptr) {
        spdlog::critical("No window created");
        return;
    }

    std::mutex surfaceMutex;
    std::condition_variable surfaceCond;
    std::optional<VkSurfaceKHR> maybeSurface = {};
    std::unique_lock surfaceLock(surfaceMutex);

    window->eventLoop()->post([&]() {
        std::unique_lock surfaceSubLock(surfaceMutex);
        maybeSurface = window->surface(instance);
        surfaceCond.notify_one();
    });

    while (!maybeSurface.has_value()) {
        surfaceCond.wait(surfaceLock);
    }

    const auto surface = maybeSurface.value();
    if (surface == VK_NULL_HANDLE) {
        spdlog::critical("No vulkan surface");
        return;
    }
    surfaceLock.unlock();

    mImpl->contentScale = window->contentScale();

    VkPhysicalDeviceVulkan12Features features12 = {};
    features12.timelineSemaphore = VK_TRUE;

    bool hasSeparateTransfer = true;
    vkb::PhysicalDeviceSelector selector { instance };
    auto maybePhysicalDevice = selector
        .set_minimum_version(1, 2)
        .set_required_features_12(features12)
        .add_required_extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)
        .require_separate_transfer_queue()
        .require_present()
        .set_surface(surface)
        .select();
    if (!maybePhysicalDevice) {
        vkb::PhysicalDeviceSelector selector { instance };
        maybePhysicalDevice = selector
            .set_minimum_version(1, 2)
            .set_required_features_12(features12)
            .add_required_extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)
            .require_present()
            .set_surface(surface)
            .select();
        if (maybePhysicalDevice) {
            // verify that we have separate queues where one can do graphics and a separate queue can do transfer
            auto families = maybePhysicalDevice.value().get_queue_families();
            bool hasGraphics = false;
            bool hasTransfer = false;
            for (auto family : families) {
                if (!hasGraphics && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                    hasGraphics = true;
                } else if (!hasTransfer && (family.queueFlags & VK_QUEUE_TRANSFER_BIT)) {
                    hasTransfer = true;
                }
            }
            if (!hasGraphics || !hasTransfer) {
                spdlog::critical("Unable to select vulkan physical device: no_suitable_device");
                return;
            }
            hasSeparateTransfer = false;
        } else {
            spdlog::critical("Unable to select vulkan physical device: {}", maybePhysicalDevice.error().message());
            return;
        }
    }

    vkb::DeviceBuilder deviceBuilder { maybePhysicalDevice.value() };
    auto maybeDevice = deviceBuilder.build();
    if (!maybeDevice) {
        spdlog::critical("Unable to create vulkan device: {}", maybeDevice.error().message());
        return;
    }

    mImpl->vkbDevice = maybeDevice.value();
    mImpl->device = mImpl->vkbDevice.device;
    volkLoadDevice(mImpl->device);

    spurv_vk::vkCmdPushDescriptorSetKHR = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(vkGetDeviceProcAddr(mImpl->device, "vkCmdPushDescriptorSetKHR"));
    if (spurv_vk::vkCmdPushDescriptorSetKHR == nullptr) {
        spdlog::critical("No vkCmdPushDescriptorSetKHR function");
    }

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

    auto maybeTransferQueue = get_transfer_queue(mImpl->vkbDevice, hasSeparateTransfer);
    if (!maybeTransferQueue) {
        spdlog::critical("Unable to get vulkan transfer queue: {}", maybeTransferQueue.error().message());
        return;
    }
    auto maybeTransferQueueIndex = get_transfer_queue_index(mImpl->vkbDevice, hasSeparateTransfer);
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
    VmaVulkanFunctions vmaVulkanFuncs = {};
    vmaVulkanFuncs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vmaVulkanFuncs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    vmaVulkanFuncs.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vmaVulkanFuncs.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vmaVulkanFuncs.vkAllocateMemory = vkAllocateMemory;
    vmaVulkanFuncs.vkFreeMemory = vkFreeMemory;
    vmaVulkanFuncs.vkMapMemory = vkMapMemory;
    vmaVulkanFuncs.vkUnmapMemory = vkUnmapMemory;
    vmaVulkanFuncs.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vmaVulkanFuncs.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vmaVulkanFuncs.vkBindBufferMemory = vkBindBufferMemory;
    vmaVulkanFuncs.vkBindImageMemory = vkBindImageMemory;
    vmaVulkanFuncs.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vmaVulkanFuncs.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vmaVulkanFuncs.vkCreateBuffer = vkCreateBuffer;
    vmaVulkanFuncs.vkDestroyBuffer = vkDestroyBuffer;
    vmaVulkanFuncs.vkCreateImage = vkCreateImage;
    vmaVulkanFuncs.vkDestroyImage = vkDestroyImage;
    vmaVulkanFuncs.vkCmdCopyBuffer = vkCmdCopyBuffer;
    vmaVulkanFuncs.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;
    vmaVulkanFuncs.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;
    vmaVulkanFuncs.vkBindBufferMemory2KHR = vkBindBufferMemory2;
    vmaVulkanFuncs.vkBindImageMemory2KHR = vkBindImageMemory2;
    vmaVulkanFuncs.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;
    vmaVulkanFuncs.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
    vmaVulkanFuncs.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;

    VmaAllocatorCreateInfo vmaInfo = {};
    vmaInfo.physicalDevice = maybePhysicalDevice.value();
    vmaInfo.device = mImpl->device;
    vmaInfo.instance = instance;
    vmaInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    vmaInfo.pVulkanFunctions = &vmaVulkanFuncs;

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

    VkSamplerCreateInfo textSamplerInfo = {};
    textSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    textSamplerInfo.minFilter = VK_FILTER_LINEAR;
    textSamplerInfo.magFilter = VK_FILTER_LINEAR;
    textSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    textSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    textSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    textSamplerInfo.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    textSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    textSamplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    VK_CHECK_SUCCESS(vkCreateSampler(mImpl->device, &textSamplerInfo, nullptr, &mImpl->textSampler));

    VkDescriptorSetLayoutBinding textUniformBinding[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        }
    };

    VkDescriptorSetLayoutCreateInfo textUniformLayoutInfo = {};
    textUniformLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    textUniformLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    textUniformLayoutInfo.bindingCount = 4;
    textUniformLayoutInfo.pBindings = textUniformBinding;
    VK_CHECK_SUCCESS(vkCreateDescriptorSetLayout(mImpl->device, &textUniformLayoutInfo, nullptr, &mImpl->textUniformLayout));

    VkPipelineLayoutCreateInfo textPipelineLayoutInfo = {};
    textPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    textPipelineLayoutInfo.setLayoutCount = 1;
    textPipelineLayoutInfo.pSetLayouts = &mImpl->textUniformLayout;
    VK_CHECK_SUCCESS(vkCreatePipelineLayout(mImpl->device, &textPipelineLayoutInfo, nullptr, &mImpl->textPipelineLayout));

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
        mImpl->scaledWidth = static_cast<uint32_t>(rect.width * mImpl->contentScale.width);
        mImpl->scaledHeight = static_cast<uint32_t>(rect.height * mImpl->contentScale.height);
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
                mImpl->scaledWidth = static_cast<uint32_t>(width * mImpl->contentScale.width);
                mImpl->scaledHeight = static_cast<uint32_t>(height * mImpl->contentScale.height);
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
        .set_desired_extent(mImpl->scaledWidth, mImpl->scaledHeight)
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_RELAXED_KHR)
        .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .build();
    if (!maybeSwapchain) {
        spdlog::critical("Unable to create swapchain: {}", maybeSwapchain.error().message());
        mImpl->vkbSwapchain.swapchain = VK_NULL_HANDLE;
        return false;
    }
    afterCurrentFrame([
        impl = mImpl,
        fbos = std::move(mImpl->swapchainFramebuffers),
        imageViews = std::move(mImpl->imageViews),
        pass = mImpl->swapchainRenderPass,
        textPipeline = mImpl->textPipeline,
        swapchain = std::move(mImpl->vkbSwapchain)
        ]() mutable -> void {
        for (auto framebuffer : fbos) {
            vkDestroyFramebuffer(impl->device, framebuffer, nullptr);
        }
        if (pass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(impl->device, pass, nullptr);
        }
        if (textPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(impl->device, textPipeline, nullptr);
        }
        swapchain.destroy_image_views(imageViews);
        vkb::destroy_swapchain(swapchain);
    });
    mImpl->swapchainFramebuffers.clear();
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
    spdlog::info("created swapchain, wanted {}x{}, got {}x{}",
                 mImpl->scaledWidth, mImpl->scaledHeight,
                 mImpl->vkbSwapchain.extent.width, mImpl->vkbSwapchain.extent.height);
    if (mImpl->vkbSwapchain.extent.width != mImpl->scaledWidth
        || mImpl->vkbSwapchain.extent.height != mImpl->scaledHeight) {
        mImpl->scaledWidth = mImpl->vkbSwapchain.extent.width;
        mImpl->scaledHeight = mImpl->vkbSwapchain.extent.height;
        mImpl->width = static_cast<uint32_t>(mImpl->scaledWidth / mImpl->contentScale.width);
        mImpl->height = static_cast<uint32_t>(mImpl->scaledHeight / mImpl->contentScale.height);
    }

    VkAttachmentReference colorAttachmentReference = {};
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription renderPassSubpass = {};
    renderPassSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    renderPassSubpass.colorAttachmentCount = 1;
    renderPassSubpass.pColorAttachments = &colorAttachmentReference;

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = mImpl->vkbSwapchain.image_format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &renderPassSubpass;
    VK_CHECK_SUCCESS(vkCreateRenderPass(mImpl->device, &renderPassInfo, nullptr, &mImpl->swapchainRenderPass));

    VkVertexInputBindingDescription vertexBindingDescription = {};
    vertexBindingDescription.binding = 0;
    vertexBindingDescription.stride = 4 * sizeof(float);
    vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> vertexAttributeDescriptions = {};
    vertexAttributeDescriptions[0].binding = 0;
    vertexAttributeDescriptions[0].location = 0;
    vertexAttributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    vertexAttributeDescriptions[0].offset = 0;
    vertexAttributeDescriptions[1].binding = 0;
    vertexAttributeDescriptions[1].location = 1;
    vertexAttributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    vertexAttributeDescriptions[1].offset = 2 * sizeof(float);

    GraphicsPipelineCreateInfo textPipelineInfo = {};
    textPipelineInfo.vertexShader = "bin/shaders/text-vs.spv";
    textPipelineInfo.fragmentShader = "bin/shaders/text-fs.spv";
    textPipelineInfo.renderPass = mImpl->swapchainRenderPass;
    textPipelineInfo.layout = mImpl->textPipelineLayout;
    textPipelineInfo.vertexBindingDescriptionCount = 1;
    textPipelineInfo.pVertexBindingDescriptions = &vertexBindingDescription;
    textPipelineInfo.vertexAttributeDescriptionCount = 2;
    textPipelineInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();
    auto maybeTextPipeline = createGraphicsPipeline(mImpl->device, textPipelineInfo);
    if (maybeTextPipeline.hasError()) {
        spdlog::critical("Failed to create graphics pipeline: {}", std::get<Error>(maybeTextPipeline.data).message);
        abort();
    }
    mImpl->textPipeline = std::get<VkPipeline>(maybeTextPipeline.data);

    mImpl->swapchainFramebuffers.resize(mImpl->imageViews.size());
    for (std::size_t viewIdx = 0; viewIdx < mImpl->imageViews.size(); ++viewIdx) {
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = mImpl->swapchainRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &mImpl->imageViews[viewIdx];
        framebufferInfo.width = mImpl->scaledWidth;
        framebufferInfo.height = mImpl->scaledHeight;
        framebufferInfo.layers = 1;
        VK_CHECK_SUCCESS(vkCreateFramebuffer(mImpl->device, &framebufferInfo, nullptr, &mImpl->swapchainFramebuffers[viewIdx]));
    }

    mImpl->semaphores.resize(mImpl->imageViews.size());
    for (auto& sem : mImpl->semaphores) {
        sem.setDevice(mImpl->device);
    }

    mImpl->recreateUniformBuffers();

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

    if (!mImpl->inFrameCallbacks.empty()) {
        for (auto& cb : mImpl->inFrameCallbacks) {
            cb(cmdbuffer);
        }
        mImpl->inFrameCallbacks.clear();
    }

    if (!mImpl->textLines.empty() && mImpl->textVBOs.empty()) {
        mImpl->generateVBOs(cmdbuffer);
    }
    if (!mImpl->textVBOs.empty() && mImpl->geomUniformBuffer != VK_NULL_HANDLE && mImpl->colorUniformBuffer != VK_NULL_HANDLE) {
        // draw some text
        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = mImpl->swapchainRenderPass;
        renderPassInfo.framebuffer = mImpl->swapchainFramebuffers[mImpl->currentSwapchainImage];
        renderPassInfo.renderArea = {
            { 0, 0 },
            { mImpl->scaledWidth, mImpl->scaledHeight }
        };
        const VkClearValue clear0 = {};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clear0;
        vkCmdBeginRenderPass(cmdbuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mImpl->textPipeline);

        for (const auto& lines : mImpl->textVBOs) {
            VkViewport viewport = {};
            viewport.x = 0.f;
            viewport.y = 0.f;
            viewport.width = static_cast<float>(mImpl->scaledWidth);
            viewport.height = static_cast<float>(mImpl->scaledHeight);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmdbuffer, 0, 1, &viewport);

            VkRect2D scissor = {};
            scissor.offset = { 0, 0 };
            scissor.extent = { mImpl->scaledWidth, mImpl->scaledHeight };
            vkCmdSetScissor(cmdbuffer, 0, 1, &scissor);

            for (const auto& line : lines) {
                if (line.view() == VK_NULL_HANDLE) {
                    continue;
                }

                VkBuffer vtxbuf = line.buffer();
                VkDeviceSize vtxoff = 0;
                vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &vtxbuf, &vtxoff);

                std::array<VkDescriptorBufferInfo, 2> bufferDescriptorInfos = {};
                std::array<VkDescriptorImageInfo, 2> imageDescriptorInfos = {};
                std::array<VkWriteDescriptorSet, 4> writeDescriptorSets = {};

                bufferDescriptorInfos[0] = {
                    .buffer = mImpl->geomUniformBuffer,
                    .offset = 0,
                    .range = VK_WHOLE_SIZE
                };
                bufferDescriptorInfos[1] = {
                    .buffer = mImpl->colorUniformBuffer,
                    .offset = 0,
                    .range = VK_WHOLE_SIZE
                };

                imageDescriptorInfos[0] = {
                    .sampler = mImpl->textSampler,
                    .imageView = VK_NULL_HANDLE,
                    .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED
                };
                imageDescriptorInfos[1] = {
                    .sampler = VK_NULL_HANDLE,
                    .imageView = line.view(),
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };

                writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSets[0].dstBinding = 0;
                writeDescriptorSets[0].descriptorCount = 1;
                writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                writeDescriptorSets[0].pBufferInfo = &bufferDescriptorInfos[0];

                writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSets[1].dstBinding = 1;
                writeDescriptorSets[1].descriptorCount = 1;
                writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                writeDescriptorSets[1].pBufferInfo = &bufferDescriptorInfos[1];

                writeDescriptorSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSets[2].dstBinding = 2;
                writeDescriptorSets[2].descriptorCount = 1;
                writeDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                writeDescriptorSets[2].pImageInfo = &imageDescriptorInfos[0];

                writeDescriptorSets[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSets[3].dstBinding = 3;
                writeDescriptorSets[3].descriptorCount = 1;
                writeDescriptorSets[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                writeDescriptorSets[3].pImageInfo = &imageDescriptorInfos[1];

                spurv_vk::vkCmdPushDescriptorSetKHR(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mImpl->textPipelineLayout, 0, 4, writeDescriptorSets.data());

                vkCmdDraw(cmdbuffer, line.size(), 1, 0, 0);
            }
        }

        vkCmdEndRenderPass(cmdbuffer);
    } else {
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
    }

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
        mImpl->graphicsTimeline.value,
        0
    };
    VkSemaphore waitSems[] = {
        semCurrent,
        mImpl->graphicsTimeline.semaphore,
        VK_NULL_HANDLE
    };
    const static VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // swapchain image
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // potential image transfer/upload
        VK_PIPELINE_STAGE_TRANSFER_BIT // potential transfer operation
    };

    {
        std::unique_lock lock(mMutex);
        if (!mImpl->afterTransfers.empty()) {
            timelineSemValues[2] = mImpl->highestAfterTransfer;
            waitSems[2] = mImpl->transferTimeline.semaphore;

            mImpl->highestAfterTransfer = 0;
            // attach all callback to the current fence
            const auto end = mImpl->afterFrameCallbacks.size();
            mImpl->afterFrameCallbacks.resize(end + mImpl->afterTransfers.size());
            std::transform(mImpl->afterTransfers.cbegin(), mImpl->afterTransfers.cend(), mImpl->afterFrameCallbacks.begin() + end, [](auto entry) {
                return entry.second;
            });
            mImpl->afterTransfers.clear();
        }
    }

    VkTimelineSemaphoreSubmitInfo timelineInfo = {};
    timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timelineInfo.waitSemaphoreValueCount = waitSems[2] == VK_NULL_HANDLE ? 2 : 3;
    timelineInfo.pWaitSemaphoreValues = timelineSemValues;
    timelineInfo.signalSemaphoreValueCount = 0;
    timelineInfo.pSignalSemaphoreValues = nullptr;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = &timelineInfo;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.waitSemaphoreCount = waitSems[2] == VK_NULL_HANDLE ? 2 : 3;
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
        fenceInfo.callbacks = std::move(mImpl->afterFrameCallbacks);
        fenceInfo.valid = true;
        mImpl->afterFrameCallbacks.clear();
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
    mImpl->afterFrameCallbacks.push_back(std::move(func));
}

void Renderer::afterTransfer(uint64_t value, std::function<void()>&& func)
{
    std::unique_lock lock(mMutex);
    mImpl->afterTransfers.push_back(std::make_pair(value, std::move(func)));
    if (value > mImpl->highestAfterTransfer) {
        mImpl->highestAfterTransfer = value;
    }
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
    imgMemBarrier.srcAccessMask = 0;
    imgMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        cmdbuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imgMemBarrier);

    VK_CHECK_SUCCESS(vkEndCommandBuffer(cmdbuffer));

    // submit and signal the graphics timeline
    uint64_t semSignal = ++mImpl->graphicsTimeline.value;
    VkTimelineSemaphoreSubmitInfo timelineInfo = {};
    timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timelineInfo.signalSemaphoreValueCount = 1;
    timelineInfo.pSignalSemaphoreValues = &semSignal;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = &timelineInfo;
    const static VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &mImpl->graphicsTimeline.semaphore;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdbuffer;
    VK_CHECK_SUCCESS(vkQueueSubmit(mImpl->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));

    spdlog::info("glyphs created and submitted");
    afterCurrentFrame([impl = mImpl, atlas = created.atlas, glyphs = std::move(created.glyphs), image = created.image]() -> void {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.components = {
            .r = VK_COMPONENT_SWIZZLE_R,
            .g = VK_COMPONENT_SWIZZLE_G,
            .b = VK_COMPONENT_SWIZZLE_B,
            .a = VK_COMPONENT_SWIZZLE_A
        };
        viewInfo.subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };
        VkImageView view = VK_NULL_HANDLE;
        VK_CHECK_SUCCESS(vkCreateImageView(impl->device, &viewInfo, nullptr, &view));

        for (auto g : glyphs) {
            auto box = atlas->glyphBox(g.getIndex());
            assert(box != nullptr);
            box->box = g;
            box->image = image;
            box->view = view;
        }
        impl->textVBOs.clear();
        spdlog::info("glyphs ready for use");
    });
}
