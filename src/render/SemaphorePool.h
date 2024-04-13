#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace spurv {

class SemaphorePool
{
public:
    SemaphorePool(VkDevice device = VK_NULL_HANDLE);
    SemaphorePool(SemaphorePool&&) = default;
    ~SemaphorePool();

    SemaphorePool& operator=(SemaphorePool&& other) = default;

    VkSemaphore* current();
    VkSemaphore* next();

    VkFence fence() const;
    void setFence(VkFence fence);

    void reset();

private:
    SemaphorePool(const SemaphorePool&) = delete;
    SemaphorePool& operator=(const SemaphorePool&&) = delete;

private:
    VkDevice mDevice;
    std::vector<VkSemaphore> mSemaphores;
    uint32_t mCurrent = 0;
    VkFence mFence = VK_NULL_HANDLE;
};

inline SemaphorePool::SemaphorePool(VkDevice device)
    : mDevice(device)
{
}

inline VkFence SemaphorePool::fence() const
{
    return mFence;
}

inline void SemaphorePool::setFence(VkFence fence)
{
    mFence = fence;
}

inline void SemaphorePool::reset()
{
    mCurrent = 0;
}

} // namespace spurv
