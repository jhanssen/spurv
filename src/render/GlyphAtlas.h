#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <filesystem>
#include <unordered_map>
#include <cstdint>

namespace spurv {

struct GlyphTimeline
{
    VkSemaphore semaphore = VK_NULL_HANDLE;
    uint64_t value = 0;
};

struct GlyphInfo
{
    msdf_atlas::GlyphBox box = {};
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

class GlyphAtlas
{
public:
    GlyphAtlas();
    ~GlyphAtlas();

    void setVulkanInfo(VkDevice device, VkQueue queue, VmaAllocator allocator);
    void setFontFile(const std::filesystem::path& path);
    uint64_t generate(char32_t from, char32_t to, const GlyphTimeline& timeline);

    const GlyphInfo* glyphBox(char32_t unicode) const;

private:
    void destroy();

private:
    VkDevice mDevice = VK_NULL_HANDLE;
    VkQueue mQueue = VK_NULL_HANDLE;
    VmaAllocator mAllocator = VK_NULL_HANDLE;
    std::filesystem::path mFontFile;
    std::unordered_map<char32_t, GlyphInfo> mGlyphs;
    static thread_local msdfgen::FreetypeHandle* tFreetype;
};

inline GlyphAtlas::GlyphAtlas()
{
}

inline GlyphAtlas::~GlyphAtlas()
{
    destroy();
}

inline void GlyphAtlas::setVulkanInfo(VkDevice device, VkQueue queue, VmaAllocator allocator)
{
    mDevice = device;
    mQueue = queue;
    mAllocator = allocator;
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
