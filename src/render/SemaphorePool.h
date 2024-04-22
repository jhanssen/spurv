#pragma once

#include <volk.h>
#include <vector>
#include <cstdint>

namespace spurv {

class SemaphorePool
{
public:
    SemaphorePool();
    SemaphorePool(SemaphorePool&&) = default;
    ~SemaphorePool();

    SemaphorePool& operator=(SemaphorePool&& other) = default;

    VkSemaphore current();
    VkSemaphore next();

    VkFence fence() const;
    void setFence(VkFence fence);
    void setDevice(VkDevice device);

    void reset();

    void recreate();

private:
    SemaphorePool(const SemaphorePool&) = delete;
    SemaphorePool& operator=(const SemaphorePool&&) = delete;

private:
    VkDevice mDevice = VK_NULL_HANDLE;
    VkFence mFence = VK_NULL_HANDLE;
    std::vector<VkSemaphore> mSemaphores;
    uint32_t mCurrent = 0;
};

inline SemaphorePool::SemaphorePool()
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

inline void SemaphorePool::setDevice(VkDevice device)
{
    mDevice = device;
}

inline void SemaphorePool::reset()
{
    mCurrent = 0;
}

} // namespace spurv
