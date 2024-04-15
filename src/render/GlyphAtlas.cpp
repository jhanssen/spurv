#include "GlyphAtlas.h"
#include "Renderer.h"
#include <ThreadPool.h>
#include <VulkanCommon.h>
#include <vector>

using namespace spurv;

thread_local msdfgen::FreetypeHandle* GlyphAtlas::tFreetype = nullptr;

uint64_t GlyphAtlas::generate(char32_t from, char32_t to, const GlyphTimeline& timeline)
{
    std::vector<char32_t> missing;
    missing.reserve(to - from);
    // insert placeholders for missing glyphs
    for (auto g = from; g < to; ++g) {
        auto it = mGlyphs.find(g);
        if (it == mGlyphs.end()) {
            mGlyphs.insert(std::make_pair(g, GlyphInfo {}));
            missing.push_back(g);
        }
    }

    if (tFreetype == nullptr) {
        tFreetype = msdfgen::initializeFreetype();
        if (tFreetype == nullptr) {
            return timeline.value;
        }
    }
    auto pool = ThreadPool::mainThreadPool();
    pool->post([
        this, fontFile = mFontFile, ft = tFreetype, device = mDevice, queue = mQueue, allocator = mAllocator,
        missing = std::move(missing), sem = timeline.semaphore, semVal = timeline.value
        ]() mutable {
        if (auto font = msdfgen::loadFont(ft, fontFile.c_str())) {
            std::vector<msdf_atlas::GlyphGeometry> glyphs;
            msdf_atlas::FontGeometry fontGeometry(&glyphs);
            msdf_atlas::Charset charSet;
            for (auto g : missing) {
                charSet.add(g);
            }
            fontGeometry.loadCharset(font, 1.0, charSet);
            const double maxCornerAngle = 3.0;
            for (auto& glyph : glyphs) {
                glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, maxCornerAngle, 0);
            }
            msdf_atlas::TightAtlasPacker packer;
            packer.setDimensionsConstraint(msdf_atlas::DimensionsConstraint::SQUARE);
            packer.setMinimumScale(24.0);
            packer.setPixelRange(2.0);
            packer.setMiterLimit(1.0);
            packer.pack(glyphs.data(), glyphs.size());
            int width = 0, height = 0;
            packer.getDimensions(width, height);
            msdf_atlas::ImmediateAtlasGenerator<
                float, // pixel type of buffer for individual glyphs depends on generator function
                3, // number of atlas color channels
                &msdf_atlas::msdfGenerator, // function to generate bitmaps for individual glyphs
                msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 3> // class that stores the atlas bitmap
                > generator(width, height);
            msdf_atlas::GeneratorAttributes attributes;
            generator.setAttributes(attributes);
            generator.setThreadCount(1);
            generator.generate(glyphs.data(), glyphs.size());
            auto bitmap = static_cast<msdfgen::BitmapConstRef<msdf_atlas::byte, 3>>(generator.atlasStorage());

            // create a vulkan buffer
            VkBufferCreateInfo bufferInfo = {};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = bitmap.width * bitmap.height * 3;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            VmaAllocationCreateInfo bufferAllocationInfo = {};
            bufferAllocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            VkBuffer buffer = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;
            VK_CHECK_SUCCESS(vmaCreateBuffer(allocator, &bufferInfo, &bufferAllocationInfo, &buffer, &allocation, nullptr));

            void* data;
            VK_CHECK_SUCCESS(vmaMapMemory(allocator, allocation, &data));
            ::memcpy(data, bitmap.pixels, bitmap.width * bitmap.height * 3);
            vmaUnmapMemory(allocator, allocation);

            Renderer::instance()->afterCurrentFrame([allocator, buffer, allocation]() {
                vmaDestroyBuffer(allocator, buffer, allocation);
            });
            msdfgen::destroyFont(font);
        }
    });
    return timeline.value + 1;
}

void GlyphAtlas::destroy()
{
}
