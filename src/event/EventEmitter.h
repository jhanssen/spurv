#pragma once

#include "EventLoop.h"
#include <FunctionTraits/TypeTraits.h>
#include <functional>
#include <thread>
#include <vector>
#include <cassert>

namespace spurv {

template<typename T>
using FunctionTraits = StdExt::FunctionTraits<T>;

template<typename T>
struct FunctionBuilder;

template<typename ...Ts>
struct FunctionBuilder<std::tuple<Ts...>>
{
    // consider requiring c++23 and use std::move_only_function
    using Type = std::function<void(Ts...)>;
};

template<typename T>
class EventEmitter
{
public:
    EventEmitter();
    EventEmitter(EventEmitter&&) noexcept;

    EventEmitter& operator=(EventEmitter&&);

    using ReturnType = typename FunctionTraits<T>::ReturnType;
    using ArgTypes = typename FunctionTraits<T>::ArgTypes;

    template<typename ...Args>
    void emit(Args&& ...args);

    template<typename Func>
    void connect(Func&&);

private:
    EventEmitter(const EventEmitter&) = delete;
    EventEmitter& operator=(const EventEmitter&) = delete;

private:
    std::vector<typename FunctionBuilder<ArgTypes>::Type> mFuncs;
    std::thread::id mOwnerThread;
};

template<typename T>
inline EventEmitter<T>::EventEmitter()
    : mOwnerThread(std::this_thread::get_id())
{
    static_assert(std::is_void_v<ReturnType>, "EventEmitter only supports void return types");
}

template<typename T>
inline EventEmitter<T>::EventEmitter(EventEmitter&& other) noexcept
    : mFuncs(std::move(other.mFuncs)), mOwnerThread(other.mOwnerThread)
{
    // can only move within the same thread
    assert(mOwnerThread == std::this_thread::get_id());
}

template<typename T>
inline EventEmitter<T>& EventEmitter<T>::operator=(EventEmitter&& other)
{
    mFuncs = std::move(other.mFuncs);
    mOwnerThread = other.mOwnerThread;
    // can only move within the same thread
    assert(mOwnerThread == std::this_thread::get_id());
    return *this;
}

template<typename T>
template<typename ...Args>
void EventEmitter<T>::emit(Args&& ...args)
{
    auto tuple = std::make_tuple(std::forward<Args>(args)...);
    if (mFuncs.size() == 1) {
        std::apply(mFuncs[0], std::move(tuple));
    } else {
        for (const auto& func : mFuncs) {
            std::apply(func, tuple);
        }
    }
}

template<typename T>
template<typename Func>
void EventEmitter<T>::connect(Func&& f)
{
    if (mOwnerThread == std::this_thread::get_id()) {
        mFuncs.push_back(std::move(f));
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
        mFuncs.push_back(std::move(threadFunc));
    }
}

} // namespace spurv
