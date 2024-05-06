#pragma once

#include <EventLoop.h>
#include <FunctionBuilder.h>
#include <ThreadPool.h>
#include <UnorderedDense.h>
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace spurv {

template<typename Type, typename Tuple, std::size_t Num>
class AsyncContainerTuple
{
public:
    AsyncContainerTuple(ThreadPool* pool = nullptr);

    template<typename Callback, typename ...Args>
    void request(Callback&& callback, Args&& ...args);

    template<typename ...Args>
    void remove(Args&& ...args);

    template<typename ...Args>
    void ref(Args&& ...args);

    template<typename ...Args>
    void deref(Args&& ...args);

    void clear();

private:
    void finalize(uint64_t pendingKey, Type&& result);
    void trim();

private:
    unordered_dense::map<Tuple, std::pair<Type, uint64_t>> mCached;
    unordered_dense::map<Tuple, std::pair<uint64_t, uint64_t>> mPending;
    unordered_dense::map<uint64_t, std::vector<std::function<void(const Type&)>>> mPendingCallbacks;
    uint64_t mNextPendingCallback = 0;
    ThreadPool* mPool;
};

template<typename Type, typename Tuple, std::size_t Num>
AsyncContainerTuple<Type, Tuple, Num>::AsyncContainerTuple(ThreadPool* pool)
    : mPool(pool)
{
    if (mPool == nullptr) {
        mPool = ThreadPool::mainThreadPool();
    }
}

template<typename Type, typename Tuple, std::size_t Num>
void AsyncContainerTuple<Type, Tuple, Num>::trim()
{
    auto it = mCached.begin();
    while (it != mCached.end()) {
        assert(it->second.second > 0);
        if (it->second.second == 1) {
            mCached.erase(it);
            if (mCached.size() <= Num) {
                return;
            }
        }
        ++it;
    }
}

template<typename Type, typename Tuple, std::size_t Num>
void AsyncContainerTuple<Type, Tuple, Num>::finalize(uint64_t pendingKey, Type&& result)
{
    auto pit = mPending.begin();
    while (pit != mPending.end()) {
        if (pit->second.first == pendingKey) {
            auto cit = mPendingCallbacks.find(pendingKey);
            assert(cit != mPendingCallbacks.end());
            // do this in a loop since calling the callback may result in a new element being added
            while (!cit->second.empty()) {
                std::vector<std::function<void(const Type&)>> cbs;
                std::swap(cit->second, cbs);
                for (auto& cb : cbs) {
                    cb(result);
                }
                cit = mPendingCallbacks.find(pendingKey);
                if (cit == mPendingCallbacks.end()) {
                    // callback removed us?
                    return;
                }
            }
            mCached[std::move(pit->first)] = std::make_pair(std::move(result), pit->second.second);
            mPending.erase(pit);
            mPendingCallbacks.erase(cit);
            if (mCached.size() > Num) {
                // remove first entry with a ref of 1
                trim();
            }
            return;
        }
    }
}

template<typename Type, typename Tuple, std::size_t Num>
template<typename Callback, typename ...Args>
void AsyncContainerTuple<Type, Tuple, Num>::request(Callback&& callback, Args&& ...args)
{
    auto tuple = std::make_tuple(std::forward<Args>(args)...);
    auto it = mCached.find(tuple);
    if (it != mCached.end()) {
        callback(it->second.first);
    } else {
        auto pit = mPending.find(tuple);
        if (pit != mPending.end()) {
            mPendingCallbacks[pit->second.first].push_back(std::move(callback));
        } else {
            auto pendingKey = ++mNextPendingCallback;
            mPending[tuple] = std::make_pair(pendingKey, static_cast<uint64_t>(1));
            mPendingCallbacks[pendingKey] = { std::move(callback) };
            auto loop = EventLoop::eventLoop();
            auto container = this;
            auto cb = [loop, container, pendingKey](Type&& result) {
                loop->post([container, pendingKey, result = std::move(result)]() mutable {
                    container->finalize(pendingKey, std::move(result));
                });
            };
            mPool->post([args = std::move(tuple), cb = std::move(cb)]() mutable {
                cb(std::make_from_tuple<Type>(std::move(args)));
            });
        }
    }
}

template<typename Type, typename Tuple, std::size_t Num>
template<typename ...Args>
void AsyncContainerTuple<Type, Tuple, Num>::remove(Args&& ...args)
{
    auto tuple = std::make_tuple(std::forward<Args>(args)...);
    auto it = mCached.find(tuple);
    if (it != mCached.end()) {
        mCached.erase(it);
    } else {
        auto pit = mPending.find(tuple);
        if (pit != mPending.end()) {
            mPendingCallbacks.erase(pit->second.first);
            mPending.erase(pit);
        }
    }
}

template<typename Type, typename Tuple, std::size_t Num>
void AsyncContainerTuple<Type, Tuple, Num>::clear()
{
    mCached.clear();
    mPending.clear();
    mPendingCallbacks.clear();
}

template<typename Type, typename Tuple, std::size_t Num>
template<typename ...Args>
void AsyncContainerTuple<Type, Tuple, Num>::ref(Args&& ...args)
{
    auto tuple = std::make_tuple(std::forward<Args>(args)...);
    auto it = mCached.find(tuple);
    if (it != mCached.end()) {
        ++it->second.second;
    } else {
        auto pit = mPending.find(tuple);
        if (pit != mPending.end()) {
            ++pit->second.second;
        }
    }
}

template<typename Type, typename Tuple, std::size_t Num>
template<typename ...Args>
void AsyncContainerTuple<Type, Tuple, Num>::deref(Args&& ...args)
{
    auto tuple = std::make_tuple(std::forward<Args>(args)...);
    auto it = mCached.find(tuple);
    if (it != mCached.end()) {
        if (!--it->second.second) {
            mCached.erase(it);
        }
    } else {
        auto pit = mPending.find(tuple);
        if (pit != mPending.end()) {
            if (!--pit->second.second) {
                mPendingCallbacks.erase(pit->second.first);
                mPending.erase(pit);
            }
        }
    }
}

template<typename Type, std::size_t Num, typename Sig>
using AsyncContainerSig = AsyncContainerTuple<Type, typename FunctionTraits<Sig>::ArgTypes, Num>;

template<typename Type, std::size_t Num, typename ...Args>
using AsyncContainerArgs = AsyncContainerTuple<Type, std::tuple<Args...>, Num>;

} // namespace spurv
