#include "BindingSupport.h"

#include <spdlog/spdlog.h>

namespace spurv {
namespace binding {

napi_value undefined(napi_env env)
{
    napi_value value = nullptr;
    if (napi_get_undefined(env, &value) != napi_ok) {
        return nullptr;
    }
    return value;
}

bool hasException(napi_env env)
{
    bool pending = false;
    if (napi_is_exception_pending(env, &pending) != napi_ok) {
        return false;
    }
    return pending;
}

void throwTypeError(napi_env env, const char* message)
{
    napi_throw_type_error(env, nullptr, message);
}

void throwArgCount(napi_env env, std::size_t expected)
{
    std::string message = "expected " + std::to_string(expected) + " argument(s)";
    napi_throw_type_error(env, nullptr, message.c_str());
}

std::string fromJsString(napi_env env, napi_value value)
{
    std::size_t length = 0;
    if (napi_get_value_string_utf8(env, value, nullptr, 0, &length) != napi_ok) {
        throwTypeError(env, "expected a string");
        return {};
    }
    std::string result(length + 1, '\0');
    std::size_t written = 0;
    if (napi_get_value_string_utf8(env, value, result.data(), result.size(), &written) != napi_ok) {
        throwTypeError(env, "expected a string");
        return {};
    }
    result.resize(written);
    return result;
}

double fromJsNumber(napi_env env, napi_value value)
{
    double result = 0.0;
    if (napi_get_value_double(env, value, &result) != napi_ok) {
        throwTypeError(env, "expected a number");
    }
    return result;
}

std::int32_t fromJsInt32(napi_env env, napi_value value)
{
    std::int32_t result = 0;
    if (napi_get_value_int32(env, value, &result) != napi_ok) {
        throwTypeError(env, "expected a number");
    }
    return result;
}

bool fromJsBoolean(napi_env env, napi_value value)
{
    bool result = false;
    if (napi_get_value_bool(env, value, &result) != napi_ok) {
        throwTypeError(env, "expected a boolean");
    }
    return result;
}

napi_value toJsString(napi_env env, const std::string& value)
{
    napi_value result = nullptr;
    if (napi_create_string_utf8(env, value.data(), value.size(), &result) != napi_ok) {
        return nullptr;
    }
    return result;
}

napi_value toJsNumber(napi_env env, double value)
{
    napi_value result = nullptr;
    if (napi_create_double(env, value, &result) != napi_ok) {
        return nullptr;
    }
    return result;
}

napi_value toJsBoolean(napi_env env, bool value)
{
    napi_value result = nullptr;
    if (napi_get_boolean(env, value, &result) != napi_ok) {
        return nullptr;
    }
    return result;
}

namespace {
thread_local void* gBorrowStash = nullptr;
} // namespace

void setBorrowStash(void* ptr)
{
    gBorrowStash = ptr;
}

void* takeBorrowStash()
{
    void* ptr = gBorrowStash;
    gBorrowStash = nullptr;
    return ptr;
}

} // namespace binding
} // namespace spurv

namespace spurv {

void log(LogLevel level, const std::string& message)
{
    const auto lvl = static_cast<spdlog::level::level_enum>(static_cast<int>(level));
    spdlog::log(lvl, message);
}

} // namespace spurv
