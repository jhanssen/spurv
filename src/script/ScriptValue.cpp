#include "ScriptValue.h"
#include "ScriptEngine.h"
#include <cassert>
#include <unordered_set>
#include <fmt/core.h>

namespace spurv {
namespace {
ScriptBufferSource JS_IsBufferSource(JSValueConst obj)
{
    if (JS_IsObject(obj)) {
        const JSClassID id = JS_GetClassID(obj);
        if (id != JS_INVALID_CLASS_ID) {
            ScriptEngine *engine = ScriptEngine::scriptEngine();
            const std::array<JSClassID, NumScriptBufferSources> &array = engine->bufferSourceIds();
            auto it = std::find(array.begin(), array.end(), id);
            if (it != array.end()) {
                return static_cast<ScriptBufferSource>(it - array.begin());
            }
        }
    }
    return ScriptBufferSource::Invalid;
}
} // anonymous namespace

ScriptValue::ScriptValue(JSValue value)
    : mValue(value)
{}

ScriptValue::ScriptValue(ScriptValue &&other)
    : mValue(std::move(other.mValue))
{
    other.mValue = {};
}

ScriptValue::ScriptValue(bool value)
    : mValue(JS_NewBool(ScriptEngine::scriptEngine()->context(), value))
{
}

ScriptValue::ScriptValue(int32_t value)
    : mValue(JS_NewInt32(ScriptEngine::scriptEngine()->context(), value))
{
}

ScriptValue::ScriptValue(uint32_t value)
    : mValue(JS_NewUint32(ScriptEngine::scriptEngine()->context(), value))
{
}

ScriptValue::ScriptValue(double value)
    : mValue(JS_NewFloat64(ScriptEngine::scriptEngine()->context(), value))
{
}
ScriptValue::ScriptValue(const std::string &str)
    : ScriptValue(str.c_str(), str.size())
{
}

ScriptValue::ScriptValue(const char *str)
    : mValue(JS_NewStringLen(ScriptEngine::scriptEngine()->context(), str, strlen(str)))
{
}

ScriptValue::ScriptValue(const char *str, size_t len)
    : mValue(JS_NewStringLen(ScriptEngine::scriptEngine()->context(), str, len))
{
}

ScriptValue::ScriptValue(const std::u8string &str)
    : ScriptValue(str.c_str(), str.size())
{
}

ScriptValue::ScriptValue(const char8_t *str, size_t len)
    : mValue(JS_NewStringLen(ScriptEngine::scriptEngine()->context(), reinterpret_cast<const char *>(str), len))

{
}

ScriptValue::ScriptValue(std::vector<ScriptValue> &&array)
{
    auto engine = ScriptEngine::scriptEngine();
    auto context = engine->context();
    JSValue v = JS_NewArray(context);
    ScriptValue len(static_cast<uint32_t>(array.size()));
    JS_SetProperty(context, v, engine->atoms().length, *len);
    uint32_t i = 0;
    for (ScriptValue &value : array) {
        JS_SetPropertyUint32(context, v, i++, value.leakValue());
    }
    mValue = v;
}

ScriptValue::ScriptValue(std::vector<std::pair<std::string, ScriptValue>> &&object)
{
    auto engine = ScriptEngine::scriptEngine();
    auto context = engine->context();
    JSValue v = JS_NewObject(context);
    for (std::pair<std::string, ScriptValue> &value : object) {
        JS_SetPropertyStr(context, v, value.first.c_str(), value.second.leakValue());
    }
    mValue = v;
}

ScriptValue::ScriptValue(std::vector<std::pair<JSAtom, ScriptValue>> &&object)
{
    auto engine = ScriptEngine::scriptEngine();
    auto context = engine->context();
    JSValue v = JS_NewObject(context);
    for (std::pair<JSAtom, ScriptValue> &value : object) {
        JS_SetProperty(context, v, value.first, value.second.leakValue());
    }
    mValue = v;
}

ScriptValue::ScriptValue(Function &&function)
{
    auto engine = ScriptEngine::scriptEngine();
    mValue = engine->bindFunction(std::move(function)).leakValue();
}

ScriptValue::ScriptValue(Tag tag)
{
    auto engine = ScriptEngine::scriptEngine();
    auto context = engine->context();
    if (tag == Object) {
        mValue = JS_NewObject(context);
    } else {
        mValue = JS_NewArray(context);
    }
}

ScriptValue::~ScriptValue()
{
    if (mValue) {
        JS_FreeValue(ScriptEngine::scriptEngine()->context(), *mValue);
    }
}

ScriptValue &ScriptValue::operator=(ScriptValue &&other)
{
    if (mValue) {
        JS_FreeValue(ScriptEngine::scriptEngine()->context(), *mValue);
    }
    mValue = std::move(other.mValue);
    other.mValue = {};
    return *this;
}

void ScriptValue::ref()
{
    if (mValue) {
        mValue = JS_DupValue(ScriptEngine::scriptEngine()->context(), *mValue);
    }
}

void ScriptValue::clear()
{
    if (mValue) {
        JS_FreeValue(ScriptEngine::scriptEngine()->context(), *mValue);
    }
    mValue = std::nullopt;
}

ScriptValue ScriptValue::call()
{
    if (isFunction()) {
        return ScriptValue(JS_Call(ScriptEngine::scriptEngine()->context(), *mValue, *ScriptValue::undefined(), 0, nullptr));
    }
    return ScriptValue::makeError("Not a function");
}

ScriptValue ScriptValue::call(const std::vector<ScriptValue> &args)
{
    if (isFunction()) {
        std::vector<JSValue> argv(args.size());
        size_t i = 0;
        for (const ScriptValue &val : args) {
            argv[i++] = *val;
        }
        return ScriptValue(JS_Call(ScriptEngine::scriptEngine()->context(), *mValue, *ScriptValue::undefined(),
                                   argv.size(), argv.data()));
    }
    return ScriptValue::makeError("Not a function");
}

ScriptValue ScriptValue::call(const ScriptValue &arg)
{
    if (isFunction()) {
        if (arg.isValid()) {
            JSValue value = *arg;
            return ScriptValue(JS_Call(ScriptEngine::scriptEngine()->context(), *mValue, *ScriptValue::undefined(),
                                       1, &value));
        }
        return ScriptValue(JS_Call(ScriptEngine::scriptEngine()->context(), *mValue, *ScriptValue::undefined(), 0, nullptr));
    }
    return ScriptValue::makeError("Not a function");
}

ScriptValue ScriptValue::construct()
{
    if (isConstructor()) {
        return ScriptValue(JS_CallConstructor(ScriptEngine::scriptEngine()->context(), *mValue, 0, nullptr));
    }
    return ScriptValue::makeError("Not a function");
}

ScriptValue ScriptValue::construct(const std::vector<ScriptValue> &args)
{
    if (isConstructor()) {
        std::vector<JSValue> argv(args.size());
        size_t i = 0;
        for (const ScriptValue &val : args) {
            argv[i++] = *val;
        }
        return ScriptValue(JS_CallConstructor(ScriptEngine::scriptEngine()->context(), *mValue, argv.size(), argv.data()));
    }
    return ScriptValue::makeError("Not a function");
}

ScriptValue ScriptValue::construct(const ScriptValue &arg)
{
    if (isConstructor()) {
        if (arg.isValid()) {
            JSValue value = *arg;
            return ScriptValue(JS_CallConstructor(ScriptEngine::scriptEngine()->context(), *mValue, 1, &value));
        }
        return ScriptValue(JS_CallConstructor(ScriptEngine::scriptEngine()->context(), *mValue, 0, nullptr));
    }
    return ScriptValue::makeError("Not a function");
}


ScriptValue ScriptValue::clone() const
{
    if (!mValue) {
        return {};
    }

    ScriptValue ret(JS_DupValue(ScriptEngine::scriptEngine()->context(), *mValue));
    return ret;
}

JSValue ScriptValue::leakValue()
{
    assert(mValue);
    JSValue ret = *mValue;
    mValue = {};
    return ret;
}

ScriptValue ScriptValue::makeError(std::string message)
{
    auto engine = ScriptEngine::scriptEngine();
    auto context = engine->context();
    JSValue val = JS_NewError(context);
    ScriptValue msg(message);
    JS_SetProperty(context, val, engine->atoms().message, msg.leakValue());
    return ScriptValue(val);
}

ScriptValue ScriptValue::makeArrayBuffer(const void *data, size_t byteLength)
{
    return ScriptValue(JS_NewArrayBufferCopy(ScriptEngine::scriptEngine()->context(), reinterpret_cast<const unsigned char *>(data), byteLength));
}

ScriptValue ScriptValue::undefined()
{
    return ScriptValue(JSValue { {}, JS_TAG_UNDEFINED });
}

ScriptValue ScriptValue::null()
{
    return ScriptValue(JSValue { {}, JS_TAG_NULL });
}

JSValue ScriptValue::operator*() const
{
    return *mValue;
}

bool ScriptValue::operator!() const
{
    return !mValue.has_value();
}

ScriptValue::operator bool() const
{
    return mValue.has_value();
}

void ScriptValue::forEach(std::function<void(const ScriptValue &value, int idx)> function)
{
    ScriptEngine *engine = ScriptEngine::scriptEngine();
    auto ctx = engine->context();
    ScriptValue length(JS_GetProperty(ctx, *mValue, engine->atoms().length));
    int len;
    JS_ToInt32(ctx, &len, *length);
    for (int i = 0; i < len; ++i) {
        ScriptValue item(JS_GetPropertyUint32(ctx, *mValue, i));
        function(item, i);
    }
}

ScriptValue ScriptValue::getProperty(JSAtom atom) const
{
    if (isObject()) {
        return ScriptValue(JS_GetProperty(ScriptEngine::scriptEngine()->context(), *mValue, atom));
    }
    return ScriptValue();
}

ScriptValue ScriptValue::getProperty(const std::string &name) const
{
    if (isObject()) {
        return ScriptValue(JS_GetPropertyStr(ScriptEngine::scriptEngine()->context(), *mValue, name.c_str()));
    }
    return ScriptValue();
}

ScriptValue ScriptValue::getPropertyIdx(uint32_t value) const
{
    if (isObject()) {
        return ScriptValue(JS_GetPropertyUint32(ScriptEngine::scriptEngine()->context(), *mValue, value));
    }
    return ScriptValue();
}

Result<void> ScriptValue::setProperty(JSAtom atom, ScriptValue &&value)
{
    ScriptEngine *engine = ScriptEngine::scriptEngine();
    auto ctx = engine->context();
    if (value && isObject()) {
        if (JS_SetProperty(ctx, *mValue, atom, *value) == 1) {
            value.leakValue();
            return {};
        }
        auto name = JS_AtomToCString(ctx, atom);
        const auto ret = spurv::makeError(fmt::format("Failed to set property {}", name));
        JS_FreeCString(ctx, name);
        return ret;
    }
    auto name = JS_AtomToCString(ctx, atom);
    const auto ret = spurv::makeError(fmt::format("Failed to set property {}, value is not an object", name));
    JS_FreeCString(ctx, name);
    return ret;
}

Result<void> ScriptValue::setProperty(const std::string &name, ScriptValue &&value)
{
    if (value && isObject()) {
        ScriptEngine *engine = ScriptEngine::scriptEngine();
        auto ctx = engine->context();
        if (JS_SetPropertyStr(ctx, *mValue, name.c_str(), *value) == 1) {
            value.leakValue();
            return {};
        }
        return spurv::makeError(fmt::format("Failed to set property {}", name));
    }
    return spurv::makeError(fmt::format("Failed to set property {}, value is not an object", name));
}

Result<void> ScriptValue::setPropertyIdx(uint32_t idx, ScriptValue &&value)
{
    if (value && isArray()) {
        ScriptEngine *engine = ScriptEngine::scriptEngine();
        auto ctx = engine->context();
        if (JS_SetProperty(ctx, *mValue, idx, *value) == 1) {
            value.leakValue();
            return {};
        }
        return spurv::makeError(fmt::format("Failed to set property {}", idx));
    }
    return spurv::makeError(fmt::format("Failed to set property {}, value is not an array", idx));
}

bool ScriptValue::isStrictObject() const
{
    return isObject() && !isArray() && !isFunction() && !isArrayBuffer() && !isTypedArray() && !isError() && !isException();
}

bool ScriptValue::isArray() const
{
    return mValue && JS_IsArray(ScriptEngine::scriptEngine()->context(), *mValue);
}

bool ScriptValue::isArrayBuffer() const
{
    return mValue && JS_IsBufferSource(*mValue) == ScriptBufferSource::ArrayBuffer;
}

bool ScriptValue::isTypedArray() const
{
    if (mValue) {
        auto type = JS_IsBufferSource(*mValue);
        return type != ScriptBufferSource::ArrayBuffer && type != ScriptBufferSource::Invalid;
    }
    return false;
}

bool ScriptValue::isBigNum() const
{
    return isBigInt() || isBigFloat() || isBigDecimal();
}

bool ScriptValue::isBigInt() const
{
    return mValue && JS_IsBigInt(ScriptEngine::scriptEngine()->context(), *mValue);
}

bool ScriptValue::isFunction() const
{
    return mValue && JS_IsFunction(ScriptEngine::scriptEngine()->context(), *mValue);
}

bool ScriptValue::isConstructor() const
{
    return mValue && JS_IsConstructor(ScriptEngine::scriptEngine()->context(), *mValue);
}

bool ScriptValue::isError() const
{
    return mValue && JS_IsError(ScriptEngine::scriptEngine()->context(), *mValue);
}

Result<std::pair<unsigned char *, std::size_t>> ScriptValue::arrayBufferData() const
{
    if (isArrayBuffer()) {
        std::size_t p;
        unsigned char *ret = JS_GetArrayBuffer(ScriptEngine::scriptEngine()->context(), &p, *mValue);
        return std::make_pair(ret, p);
    }

    if (isTypedArray()) {
        auto context = ScriptEngine::scriptEngine()->context();
        std::size_t byteOffset, byteLen;
        ScriptValue buffer(JS_GetTypedArrayBuffer(context, *mValue, &byteOffset, &byteLen, nullptr));
        assert(buffer.isArrayBuffer());
        std::size_t p;
        unsigned char *ret = JS_GetArrayBuffer(context, &p, *buffer);
        return std::make_pair(ret + byteOffset, p);
    }

    return spurv::makeError("Invalid type for arrayBufferData");
}

Result<ScriptValue> ScriptValue::typedArrayBuffer() const
{
    if (isTypedArray()) {
        auto context = ScriptEngine::scriptEngine()->context();
        ScriptValue buffer(JS_GetTypedArrayBuffer(context, *mValue, nullptr, nullptr, nullptr));
        if (buffer.isArrayBuffer()) {
            return buffer;
        }
    }

    return spurv::makeError("Invalid type for typedArrayBuffer");
}

Result<std::size_t> ScriptValue::length() const
{
    ScriptEngine *engine = ScriptEngine::scriptEngine();
    if (isArray() || isString()) {
        ScriptValue length(JS_GetProperty(engine->context(), *mValue, engine->atoms().length));
        int len;
        JS_ToInt32(engine->context(), &len, *length);
        return len;
    } else if (isArrayBuffer() || isTypedArray()) {
        ScriptValue length(JS_GetProperty(engine->context(), *mValue, engine->atoms().byteLength));
        int len;
        JS_ToInt32(engine->context(), &len, *length);
        return len;
    }
    return spurv::makeError("Invalid type for length");
}

Result<bool> ScriptValue::toBool() const
{
    if (mValue) {
        // ### this will cast to bool, is that right?
        return JS_ToBool(ScriptEngine::scriptEngine()->context(), *mValue);
    }
    return spurv::makeError("Invalid type for toBool");
}

Result<std::string> ScriptValue::toString() const
{
    if (mValue) {
        auto context = ScriptEngine::scriptEngine()->context();
        std::size_t len;
        const char *str = JS_ToCStringLen(context, &len, *mValue);
        if (str) {
            std::string ret(str, len);
            JS_FreeCString(context, str);
            return ret;
        }
    }
    return spurv::makeError("Invalid type for toString");
}

Result<double> ScriptValue::toDouble() const
{
    if (mValue) {
        double ret;
        if (!JS_ToFloat64(ScriptEngine::scriptEngine()->context(), &ret, *mValue)) {
            return ret;
        }
    }
    return spurv::makeError("Invalid type for toDouble");
}

Result<int32_t> ScriptValue::toInt() const
{
    if (mValue) {
        int32_t ret;
        if (!JS_ToInt32(ScriptEngine::scriptEngine()->context(), &ret, *mValue)) {
            return ret;
        }
    }
    return spurv::makeError("Invalid type for toInt");
}

Result<uint32_t> ScriptValue::toUint() const
{
    if (mValue) {
        uint32_t ret;
        if (!JS_ToUint32(ScriptEngine::scriptEngine()->context(), &ret, *mValue)) {
            return ret;
        }
    }
    return spurv::makeError("Invalid type for toUint");
}

Result<std::vector<ScriptValue>> ScriptValue::toArray() const
{
    if (!isArray()) {
        return spurv::makeError("Invalid type for toArray");
    }

    auto engine = ScriptEngine::scriptEngine();
    auto ctx = engine->context();
    ScriptValue length(JS_GetProperty(ctx, *mValue, engine->atoms().length));
    int len;
    JS_ToInt32(ctx, &len, *length);
    std::vector<ScriptValue> ret(len);
    for (int i = 0; i < len; ++i) {
        ret[i] = ScriptValue(JS_GetPropertyUint32(ctx, *mValue, i));
    }
    return ret;
}

namespace {
template <typename T>
void forEachProperty(const ScriptValue &value, const T &t)
{
    auto context = ScriptEngine::scriptEngine()->context();
    JSPropertyEnum *properties;
    uint32_t len;
    std::unordered_set<std::string> seen;
    if (!JS_GetOwnPropertyNames(context, &properties, &len, *value, 0)) {
        for (uint32_t i=0; i<len; ++i) {
            std::string name = JS_AtomToCString(context, properties[i].atom);
            if (!seen.insert(name).second) {
                t(std::move(name), ScriptValue(JS_GetProperty(context, *value, properties[i].atom)));
            }
        }
    }

    ScriptValue proto(JS_GetPrototype(context, *value));
    while (true) {
        ScriptValue next(JS_GetPrototype(context, *proto));
        if (next.isNull()) {
            break;
        }

        if (!JS_GetOwnPropertyNames(context, &properties, &len, *proto, 0)) {
            for (uint32_t i=0; i<len; ++i) {
                std::string name = JS_AtomToCString(context, properties[i].atom);
                if (!seen.insert(name).second) {
                    t(std::move(name), ScriptValue(JS_GetProperty(context, *value, properties[i].atom)));
                }
            }
        }

        proto = std::move(next);
    }
}
} // anonymous namespace

Result<std::vector<std::pair<std::string, ScriptValue>>> ScriptValue::toObject() const
{
    if (!(isObject())) {
        return spurv::makeError("Invalid type for toObject");
    }

    std::vector<std::pair<std::string, ScriptValue>> ret;
    forEachProperty(*this, [&ret](std::string &&name, ScriptValue &&value) -> void {
        ret.emplace_back(std::move(name), std::move(value));
    });
    return ret;
}

Result<std::unordered_map<std::string, ScriptValue>> ScriptValue::toMap() const
{
    if (!(isObject())) {
        return spurv::makeError("Invalid type for toMap");
 }

    std::unordered_map<std::string, ScriptValue> ret;
    forEachProperty(*this, [&ret](std::string &&name, ScriptValue &&value) -> void {
        ret[std::move(name)] = std::move(value);
    });
    return ret;
}

} // namespace spurv
