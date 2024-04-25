#include "ThreadPool.h"
#include <fmt/core.h>
#include <Thread.h>

using namespace spurv;

std::unique_ptr<ThreadPool> ThreadPool::sMainThreadPool = {};

ThreadPool::ThreadPool()
    : ThreadPool(std::min<uint32_t>(4, std::thread::hardware_concurrency()))
{
}

ThreadPool::ThreadPool(uint32_t numThreads)
{
    // start <numThreads> threads
    for (uint32_t t = 0; t < numThreads; ++t) {
        mThreads.push_back(std::thread(&ThreadPool::thread_internal, this, t));
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
}

void ThreadPool::initializeMainThreadPool(uint32_t numThreads)
{
    sMainThreadPool = std::make_unique<ThreadPool>(numThreads > 0 ? numThreads : std::min<uint32_t>(4, std::thread::hardware_concurrency()));
}

void ThreadPool::destroyMainThreadPool()
{
    sMainThreadPool.reset();
}

void ThreadPool::thread_internal(uint32_t idx)
{
    setCurrentThreadName(fmt::format("ThreadPool {}", idx));

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
