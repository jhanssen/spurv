#pragma once

#include "EventLoop.h"
#include <FunctionBuilder.h>
#include <thread>
#include <mutex>
#include <vector>
#include <cstdint>
#include <cassert>

namespace spurv {

template<typename T>
class EventEmitter
{
public:
    using ConnectMode = EventLoop::ConnectMode;
    using ConnectKey = EventLoop::ConnectKey;

    EventEmitter();

    using ReturnType = typename FunctionTraits<T>::ReturnType;
    using ArgTypes = typename FunctionTraits<T>::ArgTypes;

    template<typename ...Args>
    void emit(Args&& ...args);

    template<typename Func>
    ConnectKey connect(Func&&, ConnectMode mode = ConnectMode::Auto);
    bool disconnect(ConnectKey key);
    void disconnectAll();

private:
    EventEmitter(EventEmitter&&) = delete;
    EventEmitter(const EventEmitter&) = delete;
    EventEmitter& operator=(EventEmitter&&) = delete;
    EventEmitter& operator=(const EventEmitter&) = delete;

private:
    std::mutex mMutex;
    std::vector<std::pair<uint32_t, typename FunctionBuilder<ArgTypes>::Type>> mFuncs;
    std::thread::id mOwnerThread;
    uint32_t mConnectKey = 0;
};

template<typename T>
inline EventEmitter<T>::EventEmitter()
    : mOwnerThread(std::this_thread::get_id())
{
    static_assert(std::is_void_v<ReturnType>, "EventEmitter only supports void return types");
}

template<typename T>
template<typename ...Args>
void EventEmitter<T>::emit(Args&& ...args)
{
    assert(mOwnerThread == std::this_thread::get_id());
    // this is not super optimal
    std::vector<std::pair<uint32_t, typename FunctionBuilder<ArgTypes>::Type>> funcs;
    {
        std::lock_guard lock(mMutex);
        if (mFuncs.empty()) {
            return;
        }
        std::swap(funcs, mFuncs);
    }
    auto tuple = std::make_tuple(std::forward<Args>(args)...);
    if (funcs.size() == 1) {
        std::apply(funcs[0].second, std::move(tuple));
    } else {
        for (const auto& func : funcs) {
            std::apply(func.second, tuple);
        }
    }
    {
        std::lock_guard lock(mMutex);
        std::swap(funcs, mFuncs);
        if (!funcs.empty()) {
            // we got new connections while we were emitting
            mFuncs.insert(mFuncs.end(), std::make_move_iterator(funcs.begin()), std::make_move_iterator(funcs.end()));
        }
    }
}

template<typename T>
template<typename Func>
typename EventEmitter<T>::ConnectKey EventEmitter<T>::connect(Func&& f, ConnectMode mode)
{
    uint32_t id;
    if ((mode == ConnectMode::Auto && mOwnerThread == std::this_thread::get_id()) || mode == ConnectMode::Direct) {
        std::lock_guard lock(mMutex);
        // the fact that ids start at 1 is by design, 0 is an invalid key (until the number wraps around I guess)
        id = ++mConnectKey;
        mFuncs.push_back(std::make_pair(id, std::move(f)));
    } else {
        // make a std::function that posts an event
        EventLoop* loop = EventLoop::eventLoop();
        assert(loop != nullptr);
        typename FunctionBuilder<ArgTypes>::Type func(std::move(f));
        typename FunctionBuilder<ArgTypes>::Type threadFunc = [loop, func = std::move(func)](auto... params) {
            auto tuple = std::make_tuple(params...);
            loop->post([func, tuple = std::move(tuple)]() {
                std::apply(func, tuple);
            });
        };
        std::lock_guard lock(mMutex);
        id = ++mConnectKey;
        mFuncs.push_back(std::make_pair(id, std::move(threadFunc)));
    }
    return id;
}

template<typename T>
bool EventEmitter<T>::disconnect(ConnectKey key)
{
    std::lock_guard lock(mMutex);
    auto it = mFuncs.begin();
    const auto end = mFuncs.end();
    while (it != end) {
        if (it->first == key) {
            mFuncs.erase(it);
            return true;
        }
        ++it;
    }
    return false;
}

template<typename T>
void EventEmitter<T>::disconnectAll()
{
    std::lock_guard lock(mMutex);
    mFuncs.clear();
}

} // namespace spurv
