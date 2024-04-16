#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <filesystem>
#include <limits>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <cstdint>

namespace spurv {

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
    uint64_t wait = 0;
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
    void generate(char32_t from, char32_t to, GlyphTimeline& timeline, VkCommandBuffer cmdbuf);

    const GlyphInfo* glyphBox(char32_t unicode) const;

private:
    struct PerThreadInfo
    {
    };

private:
    void destroy();

    PerThreadInfo* perThread();

private:
    GlyphVulkanInfo mVulkanInfo = {};
    std::filesystem::path mFontFile;
    std::unordered_map<char32_t, GlyphInfo> mGlyphs;
    std::mutex mMutex;
    std::unordered_map<std::thread::id, std::unique_ptr<PerThreadInfo>> mPerThread;
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

inline const GlyphInfo* GlyphAtlas::glyphBox(char32_t unicode) const
{
    auto it = mGlyphs.find(unicode);
    if (it == mGlyphs.end()) {
        return nullptr;
    }
    return &it->second;
}

} // namespace spurv
