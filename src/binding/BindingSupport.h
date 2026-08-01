#pragma once

#include <node_api.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

// N-API helpers used by the generated bindings. The generated code is kept
// dumb/declarative; all the tricky N-API glue lives here.

#define SPURV_NAPI_CALL(expr)                  \
    do {                                       \
        const napi_status __status = (expr);   \
        if (__status != napi_ok) {             \
            return nullptr;                    \
        }                                      \
    } while (false)

namespace spurv {
namespace binding {

napi_value undefined(napi_env env);
bool hasException(napi_env env);
void throwTypeError(napi_env env, const char* message);
void throwArgCount(napi_env env, std::size_t expected);

std::string fromJsString(napi_env env, napi_value value);
double fromJsNumber(napi_env env, napi_value value);
std::int32_t fromJsInt32(napi_env env, napi_value value);
bool fromJsBoolean(napi_env env, napi_value value);

napi_value toJsString(napi_env env, const std::string& value);
napi_value toJsNumber(napi_env env, double value);
napi_value toJsBoolean(napi_env env, bool value);

// Temporarily stash a pointer so the next constructed native object wraps it
// as a borrowed reference (no finalizer) instead of allocating a new one.
void setBorrowStash(void* ptr);
void* takeBorrowStash();

template<typename T>
void wrap(napi_env env, napi_value jsObject, T* ptr, bool owned);
template<typename T>
T* unwrap(napi_env env, napi_value jsObject);
template<typename T>
T* unwrapMaybe(napi_env env, napi_value jsObject);

} // namespace binding
} // namespace spurv

namespace spurv {

enum class LogLevel : std::int32_t
{
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Critical = 5
};

void log(LogLevel level, const std::string& message);

} // namespace spurv

namespace spurv {
namespace binding {
namespace {
template<typename T>
void finalizeDelete(napi_env env, void* data, void* hint)
{
    static_cast<void>(env);
    static_cast<void>(hint);
    delete static_cast<T*>(data);
}
} // namespace

template<typename T>
void wrap(napi_env env, napi_value jsObject, T* ptr, bool owned)
{
    napi_status status = napi_wrap(env, jsObject, ptr, owned ? &finalizeDelete<T> : nullptr, nullptr, nullptr);
    static_cast<void>(status);
}

template<typename T>
T* unwrap(napi_env env, napi_value jsObject)
{
    void* data = nullptr;
    if (napi_unwrap(env, jsObject, &data) != napi_ok || data == nullptr) {
        throwTypeError(env, "expected a native object");
        return nullptr;
    }
    return static_cast<T*>(data);
}

template<typename T>
T* unwrapMaybe(napi_env env, napi_value jsObject)
{
    napi_valuetype type = napi_undefined;
    if (napi_typeof(env, jsObject, &type) != napi_ok) {
        return nullptr;
    }
    if (type == napi_undefined || type == napi_null) {
        return nullptr;
    }
    return unwrap<T>(env, jsObject);
}

} // namespace binding
} // namespace spurv
