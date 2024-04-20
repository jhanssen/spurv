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

    VkBuffer buffer() const;

private:
    TextVBO(const TextVBO&) = delete;
    TextVBO& operator=(const TextVBO&) = delete;

private:
    std::vector<float> mMemory = {};
    std::size_t mOffset = 0;
    VmaAllocator mAllocator = VK_NULL_HANDLE;
    VkBuffer mBuffer = VK_NULL_HANDLE;
    VmaAllocation mAllocation = VK_NULL_HANDLE;
};

inline VkBuffer TextVBO::buffer() const
{
    return mBuffer;
}

} // namespace spurv
