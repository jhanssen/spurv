#pragma once

#include <FunctionTraits/TypeTraits.h>
#include <functional>
#include <vector>

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
    EventEmitter(EventEmitter&&) = default;

    EventEmitter& operator=(EventEmitter&&) = default;

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
};

template<typename T>
inline EventEmitter<T>::EventEmitter()
{
    static_assert(std::is_void_v<ReturnType>, "EventEmitter only supports void return types");
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
    mFuncs.push_back(std::move(f));
}

} // namespace spurv
