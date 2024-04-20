#include "TextVBO.h"
#include "Renderer.h"
#include <VulkanCommon.h>
#include <cassert>

using namespace spurv;

TextVBO::TextVBO(TextVBO&& other) noexcept
    : mMemory(std::move(other.mMemory)), mOffset(other.mOffset),
      mAllocator(other.mAllocator), mBuffer(other.mBuffer),
      mAllocation(other.mAllocation)
{
    other.mAllocator = VK_NULL_HANDLE;
    other.mBuffer = VK_NULL_HANDLE;
    other.mAllocation = VK_NULL_HANDLE;
}

TextVBO::~TextVBO()
{
    if (mAllocator != VK_NULL_HANDLE) {
        assert(mBuffer != VK_NULL_HANDLE);
        assert(mAllocation != VK_NULL_HANDLE);
        Renderer::instance()->afterCurrentFrame([allocator = mAllocator, buffer = mBuffer, allocation = mAllocation]() -> void {
            vmaDestroyBuffer(allocator, buffer, allocation);
        });
    }
}

TextVBO& TextVBO::operator=(TextVBO&& other) noexcept
{
    if (mAllocator != VK_NULL_HANDLE) {
        assert(mBuffer != VK_NULL_HANDLE);
        assert(mAllocation != VK_NULL_HANDLE);
        Renderer::instance()->afterCurrentFrame([allocator = mAllocator, buffer = mBuffer, allocation = mAllocation]() -> void {
            vmaDestroyBuffer(allocator, buffer, allocation);
        });
    }
    mMemory = std::move(other.mMemory);
    mOffset = other.mOffset;
    mAllocator = other.mAllocator;
    mBuffer = other.mBuffer;
    mAllocator = other.mAllocator;
    other.mAllocator = VK_NULL_HANDLE;
    other.mBuffer = VK_NULL_HANDLE;
    other.mAllocation = VK_NULL_HANDLE;
    return *this;
}

void TextVBO::add(const RectF& rect, const msdf_atlas::GlyphBox& glyph)
{
    if (mOffset + 8 >= mMemory.size()) {
        mMemory.resize(std::min<std::size_t>(std::max<std::size_t>(mMemory.size() * 2, 64), 1024));
    }
    assert(mOffset + 8 < mMemory.size());
    mMemory[mOffset + 0] = rect.x;
    mMemory[mOffset + 1] = rect.y;
    mMemory[mOffset + 2] = rect.width;
    mMemory[mOffset + 3] = rect.height;
    mMemory[mOffset + 4] = static_cast<float>(glyph.rect.x);
    mMemory[mOffset + 5] = static_cast<float>(glyph.rect.y);
    mMemory[mOffset + 6] = static_cast<float>(glyph.rect.w);
    mMemory[mOffset + 7] = static_cast<float>(glyph.rect.h);
    mOffset += 8;
}

void TextVBO::generate(VmaAllocator allocator, VkCommandBuffer cmdbuffer)
{
    if (mAllocator != VK_NULL_HANDLE) {
        assert(mBuffer != VK_NULL_HANDLE);
        assert(mAllocation != VK_NULL_HANDLE);
        Renderer::instance()->afterCurrentFrame([allocator = mAllocator, buffer = mBuffer, allocation = mAllocation]() -> void {
            vmaDestroyBuffer(allocator, buffer, allocation);
        });
    }

    // create a vulkan staging buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = mOffset * sizeof(float);
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo bufferAllocationInfo = {};
    bufferAllocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingBufferAllocation = VK_NULL_HANDLE;
    VK_CHECK_SUCCESS(vmaCreateBuffer(allocator, &bufferInfo, &bufferAllocationInfo, &stagingBuffer, &stagingBufferAllocation, nullptr));

    // copy vbo data to buffer
    void* data;
    VK_CHECK_SUCCESS(vmaMapMemory(allocator, stagingBufferAllocation, &data));
    ::memcpy(data, mMemory.data(), mOffset * sizeof(float));
    vmaUnmapMemory(allocator, stagingBufferAllocation);

    // create the vertex buffer
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation vertexBufferAllocation = VK_NULL_HANDLE;
    VK_CHECK_SUCCESS(vmaCreateBuffer(allocator, &bufferInfo, &bufferAllocationInfo, &vertexBuffer, &vertexBufferAllocation, nullptr));

    // copy staging buffer to vertex buffer
    VkBufferCopy bufferCopy = {};
    bufferCopy.size = mOffset * sizeof(float);
    vkCmdCopyBuffer(cmdbuffer, stagingBuffer, vertexBuffer, 1, &bufferCopy);

    // insert a memory barrier to ensure that the buffer copy has completed before it's used as a vertex buffer
    VkBufferMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    memoryBarrier.size = VK_WHOLE_SIZE;
    memoryBarrier.buffer = vertexBuffer;
    memoryBarrier.offset = 0;
    memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    vkCmdPipelineBarrier(cmdbuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                         0,
                         0, nullptr,
                         1, &memoryBarrier,
                         0, nullptr);

    mBuffer = vertexBuffer;
    mAllocation = vertexBufferAllocation;
    mAllocator = allocator;
    mMemory.clear();

    Renderer::instance()->afterCurrentFrame([allocator, stagingBuffer, stagingBufferAllocation]() -> void {
        vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);
    });
}
