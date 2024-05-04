#pragma once

#include <UnorderedDense.h>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <filesystem>
#include <limits>
#include <thread>
#include <mutex>
#include <cstdint>

namespace spurv {

class GlyphAtlas;

struct GlyphInfo
{
    msdf_atlas::GlyphBox box = {};
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

struct GlyphsCreated
{
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    GlyphAtlas* atlas = nullptr;
    std::vector<msdf_atlas::GlyphGeometry> glyphs = {};
};

struct GlyphTimeline
{
    VkSemaphore semaphore = VK_NULL_HANDLE;
    uint64_t value = 0;
};

struct GlyphVulkanQueue
{
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t family = std::numeric_limits<uint32_t>::max();
};

struct GlyphVulkanInfo
{
    VkDevice device = VK_NULL_HANDLE;
    GlyphVulkanQueue graphics = {};
    GlyphVulkanQueue transfer = {};
    VmaAllocator allocator = VK_NULL_HANDLE;
};

class GlyphAtlas
{
public:
    GlyphAtlas();
    ~GlyphAtlas();

    void setVulkanInfo(const GlyphVulkanInfo& info);
    void setFontFile(const std::filesystem::path& path);
    void generate(uint32_t from, uint32_t to, GlyphTimeline& timeline, VkCommandBuffer cmdbuf);
    void generate(const unordered_dense::set<uint32_t>& glyphs, GlyphTimeline& timeline, VkCommandBuffer cmdbuf);

    GlyphInfo* glyphBox(uint32_t unicode);
    const GlyphInfo* glyphBox(uint32_t unicode) const;

    uint32_t maxWidth() const;
    uint32_t maxHeight() const;

private:
    struct PerThreadInfo
    {
    };

private:
    void destroy();

    PerThreadInfo* perThread();
    void generate(msdf_atlas::Charset&& charset, GlyphTimeline& timeline, VkCommandBuffer cmdbuf);

private:
    GlyphVulkanInfo mVulkanInfo = {};
    std::filesystem::path mFontFile;
    unordered_dense::map<uint32_t, GlyphInfo> mGlyphs;
    uint32_t mMaxWidth = 0, mMaxHeight = 0;
    std::mutex mMutex;
    unordered_dense::map<std::thread::id, std::unique_ptr<PerThreadInfo>> mPerThread;
    static thread_local msdfgen::FreetypeHandle* tFreetype;
};

inline GlyphAtlas::GlyphAtlas()
{
}

inline GlyphAtlas::~GlyphAtlas()
{
    destroy();
}

inline void GlyphAtlas::setVulkanInfo(const GlyphVulkanInfo& info)
{
    mVulkanInfo = info;
}

inline void GlyphAtlas::setFontFile(const std::filesystem::path& path)
{
    mFontFile = path;
}

inline GlyphInfo* GlyphAtlas::glyphBox(uint32_t unicode)
{
    auto it = mGlyphs.find(unicode);
    if (it == mGlyphs.end()) {
        return nullptr;
    }
    return &it->second;
}

inline const GlyphInfo* GlyphAtlas::glyphBox(uint32_t unicode) const
{
    auto it = mGlyphs.find(unicode);
    if (it == mGlyphs.end()) {
        return nullptr;
    }
    return &it->second;
}

inline uint32_t GlyphAtlas::maxWidth() const
{
    return mMaxWidth;
}

inline uint32_t GlyphAtlas::maxHeight() const
{
    return mMaxHeight;
}

} // namespace spurv
