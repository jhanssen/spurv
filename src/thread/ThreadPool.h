#pragma once

#include <FunctionTraits/TypeTraits.h>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

namespace spurv {

template<typename T>
using FunctionTraits = StdExt::FunctionTraits<T>;

template<typename Func>
concept NonVoidReturn = !std::is_same_v<void, typename FunctionTraits<Func>::ReturnType>;

template<typename Func>
concept VoidReturn = std::is_same_v<void, typename FunctionTraits<Func>::ReturnType>;

class ThreadPool
{
public:
    ThreadPool();
    ThreadPool(uint32_t numThreads);
    ~ThreadPool();

    static void initializeMainThreadPool(uint32_t numThreads = 0);
    static void destroyMainThreadPool();

    bool isMainThreadPool() const;
    static ThreadPool* mainThreadPool();

    template<NonVoidReturn Func>
    std::future<typename FunctionTraits<Func>::ReturnType> post(Func&& func);

    template<VoidReturn Func>
    void post(Func&& func);

private:
    struct Task
    {
        virtual ~Task() { }
        virtual void run() = 0;
    };

    template<typename Func>
    struct PackagedTask : public Task
    {
        using ReturnType = typename FunctionTraits<Func>::ReturnType;

        PackagedTask(Func&& func)
            : task(std::forward<Func>(func))
        {
        }

        virtual void run() override
        {
            task();
        }

        std::packaged_task<ReturnType()> task;
    };

    template<typename Func>
    struct FunctionTask : public Task
    {
        FunctionTask(Func&& func)
            : task(std::forward<Func>(func))
        {
        }

        virtual void run() override
        {
            task();
        }

        std::function<void()> task;
    };

    void thread_internal();

private:
    std::mutex mMutex;
    std::condition_variable mCond;
    std::deque<std::unique_ptr<Task>> mTasks;
    std::vector<std::thread> mThreads;
    bool mStopped = false;

    static std::unique_ptr<ThreadPool> sMainThreadPool;
};

template<NonVoidReturn Func>
inline std::future<typename FunctionTraits<Func>::ReturnType> ThreadPool::post(Func&& func)
{
    static_assert(FunctionTraits<Func>::ArgCount == 0, "ThreadPool::post doesn't deal with function arguments as of now");
    auto task = std::make_unique<PackagedTask<Func>>(std::forward<Func>(func));
    auto future = task->task.get_future();
    std::lock_guard lock(mMutex);
    mTasks.push_back(std::move(task));
    mCond.notify_one();
    return future;
}

template<VoidReturn Func>
void ThreadPool::post(Func&& func)
{
    static_assert(FunctionTraits<Func>::ArgCount == 0, "ThreadPool::post doesn't deal with function arguments as of now");
    auto task = std::make_unique<FunctionTask<Func>>(std::forward<Func>(func));
    std::lock_guard lock(mMutex);
    mTasks.push_back(std::move(task));
    mCond.notify_one();
}

inline bool ThreadPool::isMainThreadPool() const
{
    return sMainThreadPool.get() == this;
}

inline ThreadPool* ThreadPool::mainThreadPool()
{
    return sMainThreadPool.get();
}

} // namespacespurv
