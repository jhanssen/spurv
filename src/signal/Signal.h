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
class Signal
{
public:
    Signal();
    Signal(Signal&&) = default;

    Signal& operator=(Signal&&) = default;

    using ReturnType = typename FunctionTraits<T>::ReturnType;
    using ArgTypes = typename FunctionTraits<T>::ArgTypes;

    template<typename ...Args>
    void emit(Args&& ...args);

    template<typename Func>
    void connect(Func&&);

private:
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;

private:
    std::vector<typename FunctionBuilder<ArgTypes>::Type> mFuncs;
};

template<typename T>
inline Signal<T>::Signal()
{
    static_assert(std::is_void_v<ReturnType>, "Signal only supports void return types");
}

template<typename T>
template<typename ...Args>
void Signal<T>::emit(Args&& ...args)
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
void Signal<T>::connect(Func&& f)
{
    mFuncs.push_back(std::move(f));
}

} // namespace spurv
