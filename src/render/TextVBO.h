#pragma once

#include <Geometry.h>
#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>

namespace spurv {

class TextVBO
{
public:
    TextVBO() = default;
    TextVBO(TextVBO&& other) noexcept;
    ~TextVBO();

    TextVBO& operator=(TextVBO&& other) noexcept;

    void add(const RectF& rect, const msdf_atlas::GlyphBox& glyph);
    void generate(VmaAllocator allocator, VkCommandBuffer cmdbuffer);
    void setView(VkImageView view);

    VkImageView view() const;
    VkBuffer buffer() const;
    uint32_t size() const;

private:
    TextVBO(const TextVBO&) = delete;
    TextVBO& operator=(const TextVBO&) = delete;

private:
    std::vector<float> mMemory = {};
    std::size_t mOffset = 0, mSize = 0;
    VmaAllocator mAllocator = VK_NULL_HANDLE;
    VmaAllocation mAllocation = VK_NULL_HANDLE;
    VkBuffer mBuffer = VK_NULL_HANDLE;
    VkImageView mView = VK_NULL_HANDLE;
};

inline void TextVBO::setView(VkImageView view)
{
    mView = view;
}

inline VkBuffer TextVBO::buffer() const
{
    return mBuffer;
}

inline uint32_t TextVBO::size() const
{
    return mSize;
}

inline VkImageView TextVBO::view() const
{
    return mView;
}

} // namespace spurv
