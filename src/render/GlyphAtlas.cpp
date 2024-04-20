#include "GlyphAtlas.h"
#include "Renderer.h"
#include <ThreadPool.h>
#include <VulkanCommon.h>
#include <vector>

using namespace spurv;

thread_local msdfgen::FreetypeHandle* GlyphAtlas::tFreetype = nullptr;

GlyphAtlas::PerThreadInfo* GlyphAtlas::perThread()
{
    std::lock_guard lock(mMutex);
    auto it = mPerThread.find(std::this_thread::get_id());
    if (it == mPerThread.end()) {
        auto perThread = new PerThreadInfo();
        mPerThread.insert(std::make_pair(std::this_thread::get_id(), std::unique_ptr<PerThreadInfo>(perThread)));
        return perThread;
    }
    return it->second.get();
}

void GlyphAtlas::generate(msdf_atlas::Charset&& charset, GlyphTimeline& timeline, VkCommandBuffer cmdbuffer)
{
    if (tFreetype == nullptr) {
        tFreetype = msdfgen::initializeFreetype();
        if (tFreetype == nullptr) {
            return;
        }
    }
    auto pool = ThreadPool::mainThreadPool();
    auto atlas = this;
    pool->post([
        atlas, fontFile = mFontFile, ft = tFreetype, vulkan = mVulkanInfo, cmdbuffer,
        charSet = std::move(charset), sem = timeline.semaphore, semVal = timeline.value
        ]() mutable {
        if (auto font = msdfgen::loadFont(ft, fontFile.c_str())) {
            spdlog::info("glyphatlas generating");
            std::vector<msdf_atlas::GlyphGeometry> glyphs;
            msdf_atlas::FontGeometry fontGeometry(&glyphs);
            fontGeometry.loadGlyphset(font, 1.0, charSet);
            const double maxCornerAngle = 3.0;
            for (auto& glyph : glyphs) {
                glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, maxCornerAngle, 0);
            }
            msdf_atlas::TightAtlasPacker packer;
            packer.setDimensionsConstraint(msdf_atlas::DimensionsConstraint::POWER_OF_TWO_RECTANGLE);
            packer.setMinimumScale(24.0);
            packer.setPixelRange(2.0);
            packer.setMiterLimit(1.0);
            packer.pack(glyphs.data(), glyphs.size());
            int width = 0, height = 0;
            packer.getDimensions(width, height);
            msdf_atlas::ImmediateAtlasGenerator<
                float, // pixel type of buffer for individual glyphs depends on generator function
                4, // number of atlas color channels
                &msdf_atlas::mtsdfGenerator, // function to generate bitmaps for individual glyphs
                msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4> // class that stores the atlas bitmap
                > generator(width, height);
            msdf_atlas::GeneratorAttributes attributes;
            generator.setAttributes(attributes);
            generator.setThreadCount(1);
            generator.generate(glyphs.data(), glyphs.size());
            spdlog::info("glyphsatlas generated {}x{}", width, height);
            auto bitmap = static_cast<msdfgen::BitmapConstRef<msdf_atlas::byte, 4>>(generator.atlasStorage());

            // create a vulkan buffer
            VkBufferCreateInfo bufferInfo = {};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = bitmap.width * bitmap.height * 4;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            VmaAllocationCreateInfo bufferAllocationInfo = {};
            bufferAllocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            VkBuffer buffer = VK_NULL_HANDLE;
            VmaAllocation bufferAllocation = VK_NULL_HANDLE;
            VK_CHECK_SUCCESS(vmaCreateBuffer(vulkan.allocator, &bufferInfo, &bufferAllocationInfo, &buffer, &bufferAllocation, nullptr));

            // copy glyph atlas to buffer
            void* data;
            VK_CHECK_SUCCESS(vmaMapMemory(vulkan.allocator, bufferAllocation, &data));
            ::memcpy(data, bitmap.pixels, bitmap.width * bitmap.height * 4);
            vmaUnmapMemory(vulkan.allocator, bufferAllocation);

            // create a vulkan image
            VkImageCreateInfo imageInfo = {};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = bitmap.width;
            imageInfo.extent.height = bitmap.height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.flags = 0;
            VmaAllocationCreateInfo imageAllocationInfo = {};
            imageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

            VkImage image = VK_NULL_HANDLE;
            VmaAllocation imageAllocation = VK_NULL_HANDLE;
            VK_CHECK_SUCCESS(vmaCreateImage(vulkan.allocator, &imageInfo, &imageAllocationInfo, &image, &imageAllocation, nullptr));

            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            VK_CHECK_SUCCESS(vkBeginCommandBuffer(cmdbuffer, &beginInfo));

            // transition image to copy-dst
            VkImageMemoryBarrier imgMemBarrier = {};
            imgMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imgMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imgMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imgMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imgMemBarrier.subresourceRange.baseMipLevel = 0;
            imgMemBarrier.subresourceRange.levelCount = 1;
            imgMemBarrier.subresourceRange.baseArrayLayer = 0;
            imgMemBarrier.subresourceRange.layerCount = 1;
            imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imgMemBarrier.image = image;
            imgMemBarrier.srcAccessMask = 0;
            imgMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            vkCmdPipelineBarrier(cmdbuffer,
                                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0, nullptr,
                                 0, nullptr,
                                 1, &imgMemBarrier);

            // copy buffer to image
            VkBufferImageCopy region = {};
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.layerCount = 1;
            region.imageExtent.width = bitmap.width;
            region.imageExtent.height = bitmap.height;
            region.imageExtent.depth = 1;

            vkCmdCopyBufferToImage(cmdbuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            // transition image to shader-read and transfer ownership to graphics
            imgMemBarrier.srcQueueFamilyIndex = vulkan.transfer.family;
            imgMemBarrier.dstQueueFamilyIndex = vulkan.graphics.family;
            imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imgMemBarrier.image = image;
            imgMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imgMemBarrier.dstAccessMask = 0;

            vkCmdPipelineBarrier(
                cmdbuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &imgMemBarrier);

            VK_CHECK_SUCCESS(vkEndCommandBuffer(cmdbuffer));

            uint64_t semWait = semVal;
            uint64_t semSignal = semVal + 1;
            VkTimelineSemaphoreSubmitInfo timelineInfo = {};
            timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            timelineInfo.waitSemaphoreValueCount = 1;
            timelineInfo.pWaitSemaphoreValues = &semWait;
            timelineInfo.signalSemaphoreValueCount = 1;
            timelineInfo.pSignalSemaphoreValues = &semSignal;
            VkSemaphore timelineSem = sem;

            // ### should do something to fix the fact that GlyphAtlas::generate calls
            // are sequential due to the next one waiting for the semaphore signalled
            // by the previous one
            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.pNext = &timelineInfo;
            const static VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &timelineSem;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &timelineSem;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmdbuffer;
            VK_CHECK_SUCCESS(vkQueueSubmit(vulkan.transfer.queue, 1, &submitInfo, VK_NULL_HANDLE));

            spdlog::info("glyphatlas copied");

            Renderer::instance()->afterTransfer(semSignal, [
                atlas, allocator = vulkan.allocator, buffer, bufferAllocation, image,
                cmdbuffer, charSet = std::move(charSet)]() mutable {
                spdlog::info("glyphatlas notifying");

                vmaDestroyBuffer(allocator, buffer, bufferAllocation);
                Renderer::instance()->glyphsCreated(GlyphsCreated {
                        cmdbuffer,
                        image,
                        atlas,
                        std::move(charSet)
                    });
            });
            msdfgen::destroyFont(font);
        }
    });

    timeline.value += 1;
}

void GlyphAtlas::generate(uint32_t from, uint32_t to, GlyphTimeline& timeline, VkCommandBuffer cmdbuffer)
{
    msdf_atlas::Charset charSet;
    // insert placeholders for missing glyphs
    for (auto g = from; g < to; ++g) {
        auto it = mGlyphs.find(g);
        if (it == mGlyphs.end()) {
            mGlyphs.insert(std::make_pair(g, GlyphInfo {}));
            charSet.add(g);
        }
    }
    if (!charSet.empty()) {
        generate(std::move(charSet), timeline, cmdbuffer);
    }
}

void GlyphAtlas::generate(const std::unordered_set<uint32_t>& glyphs, GlyphTimeline& timeline, VkCommandBuffer cmdbuffer)
{
    msdf_atlas::Charset charSet;
    for (auto g : glyphs) {
        auto it = mGlyphs.find(g);
        if (it == mGlyphs.end()) {
            mGlyphs.insert(std::make_pair(g, GlyphInfo {}));
            charSet.add(g);
        }
    }
    if (!charSet.empty()) {
        generate(std::move(charSet), timeline, cmdbuffer);
    }
}

void GlyphAtlas::destroy()
{
}
