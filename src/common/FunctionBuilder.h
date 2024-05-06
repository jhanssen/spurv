#pragma once

#include <FunctionTraits/TypeTraits.h>
#include <functional>

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

} // namespace spurv
