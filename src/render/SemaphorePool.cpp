#include "SemaphorePool.h"
#include "Renderer.h"
#include <VulkanCommon.h>

using namespace spurv;

SemaphorePool::~SemaphorePool()
{
    for (auto sem : mSemaphores) {
        vkDestroySemaphore(mDevice, sem, nullptr);
    }
}

VkSemaphore SemaphorePool::current()
{
    return mSemaphores[mCurrent - 1];
}

VkSemaphore SemaphorePool::next()
{
    if (mDevice != VK_NULL_HANDLE) {
        if(mCurrent >= mSemaphores.size()) {
            VkSemaphore semaphore;
            VkSemaphoreCreateInfo semaphoreInfo = {};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            VK_CHECK_SUCCESS(vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &semaphore));
            mSemaphores.push_back(semaphore);
        }
        return mSemaphores[mCurrent++];
    }
    return VK_NULL_HANDLE;
}

void SemaphorePool::recreate()
{
    if (mSemaphores.empty()) {
        assert(mCurrent == 0);
        assert(mFence == VK_NULL_HANDLE);
        return;
    }

    Renderer::instance()->afterCurrentFrame([device = mDevice, semaphores = std::move(mSemaphores)]() -> void {
        for (auto sem : semaphores) {
            vkDestroySemaphore(device, sem, nullptr);
        }
    });
    mSemaphores.clear();
    mCurrent = 0;
    mFence = VK_NULL_HANDLE;
}
