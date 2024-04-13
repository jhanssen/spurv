#include "SemaphorePool.h"
#include <Formatting.h>
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
