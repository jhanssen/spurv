#pragma once

#include <string>
#include <variant>

namespace spurv {

struct Error
{
    std::string message;
};

template<typename T>
struct Result
{
    Result(T t)
        : data(std::move(t))
    {}
    Result(Error err)
        : data(std::move(err))
    {}

    bool hasError() const;
    bool ok() const;

    std::variant<T, Error> data;
};

template<typename T>
inline bool Result<T>::hasError() const
{
    return std::holds_alternative<Error>(data);
}

template<typename T>
inline bool Result<T>::ok() const
{
    return std::holds_alternative<T>(data);
}

inline Error makeError(const std::string& msg)
{
    return Error { msg };
}

inline Error makeError(std::string&& msg)
{
    return Error { std::move(msg) };
}

} // namespace spurv
