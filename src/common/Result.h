#pragma once

#include <string>
#include <variant>
#include <cassert>

namespace spurv {

struct Error
{
    std::string message;
};

template<typename T>
class Result
{
public:
    Result(T t)
        : mData(std::move(t))
    {}
    Result(Error err)
        : mData(std::move(err))
    {}

    bool hasError() const;
    bool ok() const;

    const T *operator->() const;
    T *operator->();

    const T &data() const &;
    T &&data() &&;

    const Error &error() const &;
    Error &&error() &&;
private:
    std::variant<T, Error> mData;
};

template<typename T>
inline bool Result<T>::hasError() const
{
    return std::holds_alternative<Error>(mData);
}

template<typename T>
inline bool Result<T>::ok() const
{
    return std::holds_alternative<T>(mData);
}

template<typename T>
inline const T *Result<T>::operator->() const
{
    assert(ok());
    return &std::get<T>(mData);
}

template<typename T>
inline T *Result<T>::operator->()
{
    assert(ok());
    return &std::get<T>(mData);
}

template<typename T>
inline const T &Result<T>::data() const &
{
    assert(ok());
    return std::get<T>(mData);
}

template<typename T>
inline T &&Result<T>::data() &&
{
    assert(ok());
    return std::get<T>(std::move(mData));
}

template<typename T>
inline const Error &Result<T>::error() const &
{
    assert(hasError());
    return std::get<Error>(mData);
}

template<typename T>
inline Error &&Result<T>::error() &&
{
    assert(hasError());
    return std::get<Error>(std::move(mData));
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
