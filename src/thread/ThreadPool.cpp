#include "ThreadPool.h"

using namespace spurv;

ThreadPool* ThreadPool::sMainThreadPool = nullptr;

ThreadPool::ThreadPool()
    : ThreadPool(std::thread::hardware_concurrency())
{
}

ThreadPool::ThreadPool(uint32_t numThreads)
{
    if (sMainThreadPool == nullptr) {
        sMainThreadPool = this;
    }
    // start <numThreads> threads
    for (uint32_t t = 0; t < numThreads; ++t) {
        mThreads.push_back(std::thread(&ThreadPool::thread_internal, this));
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard lock(mMutex);
        mStopped = true;
        mCond.notify_all();
    }
    for (auto& t : mThreads) {
        t.join();
    }
    if (sMainThreadPool == this) {
        sMainThreadPool = nullptr;
    }
}

void ThreadPool::thread_internal()
{
    for (;;) {
        std::unique_ptr<Task> task;
        {
            std::unique_lock lock(mMutex);
            while (mTasks.empty()) {
                if (mStopped) {
                    return;
                }
                mCond.wait(lock);
            }
            // acquire our task
            task = std::move(mTasks.front());
            mTasks.pop_front();
        }
        task->run();
    }
}
