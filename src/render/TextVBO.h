#pragma once

#include <Geometry.h>
#include <TextProperty.h>
#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <cstdint>

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
    void setProperty(const TextProperty& property);
    void setFirstLine(uint64_t line);
    void setLinePosition(uint64_t pos);

    VkBuffer buffer() const;
    uint32_t size() const;
    uint64_t firstLine() const;
    uint64_t linePosition() const;

    VkImageView view() const;
    const TextProperty& property() const;

private:
    TextVBO(const TextVBO&) = delete;
    TextVBO& operator=(const TextVBO&) = delete;

private:
    std::vector<float> mMemory = {};
    std::size_t mOffset = 0, mSize = 0;
    uint64_t mFirstLine = 0, mLinePosition = 0;
    VmaAllocator mAllocator = VK_NULL_HANDLE;
    VmaAllocation mAllocation = VK_NULL_HANDLE;
    VkBuffer mBuffer = VK_NULL_HANDLE;
    VkImageView mView = VK_NULL_HANDLE;
    TextProperty mProperty = {};
};

inline void TextVBO::setView(VkImageView view)
{
    mView = view;
}

inline VkImageView TextVBO::view() const
{
    return mView;
}

inline void TextVBO::setProperty(const TextProperty& property)
{
    mProperty = property;
}

inline const TextProperty& TextVBO::property() const
{
    return mProperty;
}

inline void TextVBO::setFirstLine(uint64_t line)
{
    mFirstLine = line;
}

inline uint64_t TextVBO::firstLine() const
{
    return mFirstLine;
}

inline void TextVBO::setLinePosition(uint64_t pos)
{
    mLinePosition = pos;
}

inline uint64_t TextVBO::linePosition() const
{
    return mLinePosition;
}

inline VkBuffer TextVBO::buffer() const
{
    return mBuffer;
}

inline uint32_t TextVBO::size() const
{
    return mSize;
}


} // namespace spurv
