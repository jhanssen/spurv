#include "ScriptValue.h"
#include "ScriptEngine.h"
#include "JSExtra.h"
#include <cassert>
#include <unordered_set>
#include <fmt/core.h>

namespace spurv {
ScriptValue::ScriptValue(JSValue value)
    : mValue(value)
{}

ScriptValue::ScriptValue(ScriptValue &&other)
    : mValue(std::move(other.mValue)), mType(std::move(other.mType))
{
    other.mValue = {};
    other.mType = {};
}

ScriptValue::ScriptValue(bool value)
    : mValue(JS_NewBool(ScriptEngine::scriptEngine()->context(), value)), mType(Type::Boolean)
{
}

ScriptValue::ScriptValue(int32_t value)
    : mValue(JS_NewInt32(ScriptEngine::scriptEngine()->context(), value)), mType(Type::Int)
{
}

ScriptValue::ScriptValue(uint32_t value)
    : mValue(JS_NewUint32(ScriptEngine::scriptEngine()->context(), value)), mType(Type::Int)
{
}

ScriptValue::ScriptValue(double value)
    : mValue(JS_NewFloat64(ScriptEngine::scriptEngine()->context(), value)), mType(Type::Double)
{
}
ScriptValue::ScriptValue(const std::string &str)
    : ScriptValue(str.c_str(), str.size())
{
}

ScriptValue::ScriptValue(const char *str, size_t len)
    : mValue(JS_NewStringLen(ScriptEngine::scriptEngine()->context(), str, len)), mType(Type::String)
{
}

ScriptValue::ScriptValue(const std::u8string &str)
    : ScriptValue(str.c_str(), str.size())
{
}

ScriptValue::ScriptValue(const char8_t *str, size_t len)
    : mValue(JS_NewStringLen(ScriptEngine::scriptEngine()->context(), reinterpret_cast<const char *>(str), len)), mType(Type::String)

{
}

ScriptValue::ScriptValue(std::vector<ScriptValue> &&array)
    : mType(Type::Array)
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
    : mType(Type::Object)
{
    auto engine = ScriptEngine::scriptEngine();
    auto context = engine->context();
    JSValue v = JS_NewObject(context);
    for (std::pair<std::string, ScriptValue> &value : object) {
        JS_SetPropertyStr(context, v, value.first.c_str(), value.second.leakValue());
    }
    mValue = v;
}

ScriptValue::ScriptValue(Function &&function)
    : mType(Type::Function)
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
    mType = std::move(other.mType);
    other.mValue = {};
    other.mType = {};
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
    mType = std::nullopt;
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

ScriptValue ScriptValue::clone() const
{
    if (!mValue) {
        return {};
    }

    ScriptValue ret(JS_DupValue(ScriptEngine::scriptEngine()->context(), *mValue));
    ret.mType = mType;
    return ret;
}

JSValue ScriptValue::leakValue()
{
    assert(mValue);
    JSValue ret = *mValue;
    mValue = {};
    mType = {};
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

ScriptValue ScriptValue::getProperty(const std::string &name) const
{
    if (type() & Type::Object) {
        return ScriptValue(JS_GetPropertyStr(ScriptEngine::scriptEngine()->context(), *mValue, name.c_str()));
    }
    return ScriptValue();
}

ScriptValue ScriptValue::getProperty(uint32_t value) const
{
    if (type() & Type::Object) {
        return ScriptValue(JS_GetPropertyUint32(ScriptEngine::scriptEngine()->context(), *mValue, value));
    }
    return ScriptValue();
}

Result<void> ScriptValue::setProperty(const std::string &name, ScriptValue &&value)
{
    if (type() & Type::Object && value) {
        ScriptEngine *engine = ScriptEngine::scriptEngine();
        auto ctx = engine->context();
        if (JS_SetPropertyStr(ctx, *mValue, name.c_str(), value.leakValue()) == 0) {
            value.leakValue();
            return {};
        }
        return spurv::makeError(fmt::format("Failed to set property {}", name));
    }
    return spurv::makeError(fmt::format("Failed to set property {}, value is not an object", name));
}

Result<void> ScriptValue::setProperty(uint32_t idx, ScriptValue &&value)
{
    if (type() & Type::Array && value) {
        ScriptEngine *engine = ScriptEngine::scriptEngine();
        auto ctx = engine->context();
        if (JS_SetProperty(ctx, *mValue, idx, value.leakValue()) == 0) {
            value.leakValue();
            return {};
        }
        return spurv::makeError(fmt::format("Failed to set property {}", idx));
    }
    return spurv::makeError(fmt::format("Failed to set property {}, value is not an array", idx));
}

ScriptValue::Type ScriptValue::type() const
{
    if (!mType) {
        if (!mValue) {
            mType = Type::Invalid;
            return *mType;
        }

        auto ctx = ScriptEngine::scriptEngine()->context();

        if (JS_IsString(*mValue)) {
            mType = Type::String;
        } else if (JS_IsNumber(*mValue)) {
            if (JS_VALUE_GET_TAG(*mValue) == JS_TAG_INT) {
                mType = Type::Int;
            } else {
                mType = Type::Double;
            }
        } else if (JS_IsBool(*mValue)) {
            mType = Type::Boolean;
        } else if (JS_IsUndefined(*mValue)) {
            mType = Type::Undefined;
        } else if (JS_IsArray(ctx, *mValue)) {
            mType = Type::Array;
        } else if (JS_IsNull(*mValue)) {
            mType = Type::Null;
        } else if (JS_IsArrayBuffer(ctx, *mValue)) {
            mType = Type::ArrayBuffer;
        } else if (JS_IsTypedArray(ctx, *mValue)) {
            mType = Type::TypedArray;
        } else if (JS_IsFunction(ctx, *mValue)) {
            mType = Type::Function;
        } else if (JS_IsError(ctx, *mValue)) {
            mType = Type::Error;
        } else if (JS_IsException(*mValue)) {
            mType = Type::Exception;
        } else if (JS_IsObject(*mValue)) {
            mType = Type::Object;
        } else if (JS_IsSymbol(*mValue)) {
            mType = Type::Symbol;
        } else if (JS_IsBigInt(ctx, *mValue)) {
            mType = Type::BigInt;
        } else if (JS_IsBigFloat(*mValue)) {
            mType = Type::BigFloat;
        } else if (JS_IsBigDecimal(*mValue)) {
            mType = Type::BigDecimal;
        } else {
            assert(false);
            mType = Type::Invalid;
        }
    }
    return *mType;
}

Result<std::pair<unsigned char *, std::size_t>> ScriptValue::arrayBufferData() const
{
    switch (type()) {
    case Type::ArrayBuffer: {
        std::size_t p;
        unsigned char *ret = JS_GetArrayBuffer(ScriptEngine::scriptEngine()->context(), &p, *mValue);
        return std::make_pair(ret, p); }
    case Type::TypedArray: {
        auto context = ScriptEngine::scriptEngine()->context();
        std::size_t byteOffset, byteLen;
        ScriptValue buffer(JS_GetTypedArrayBuffer(context, *mValue, &byteOffset, &byteLen, nullptr));
        std::size_t p;
        unsigned char *ret = JS_GetArrayBuffer(context, &p, *mValue);
        if (ret) {
            return std::make_pair(ret + byteOffset, p);
        }
        return spurv::makeError("Can't get typed array data"); }
    default:
        break;
    }

    return spurv::makeError("Invalid type for arrayBufferData");
}

Result<ScriptValue> ScriptValue::typedArrayBuffer() const
{
    auto context = ScriptEngine::scriptEngine()->context();
    ScriptValue buffer(JS_GetTypedArrayBuffer(context, *mValue, nullptr, nullptr, nullptr));
    if (buffer.type() == Type::ArrayBuffer) {
        return buffer;
    }

    return spurv::makeError("Invalid type for typedArrayBuffer");
}

Result<std::size_t> ScriptValue::length() const
{
    ScriptEngine *engine = ScriptEngine::scriptEngine();
    switch (type()) {
    case Type::ArrayBuffer: {
        std::size_t p;
        if (JS_GetArrayBuffer(engine->context(), &p, *mValue)) {
            return p;
        }
        break; }
    case Type::Array:
    case Type::String: {
        ScriptValue length(JS_GetProperty(engine->context(), *mValue, engine->atoms().length));
        int len;
        JS_ToInt32(engine->context(), &len, *length);
        return len; }
    case Type::TypedArray: {
        std::size_t byteLen;
        ScriptValue buffer(JS_GetTypedArrayBuffer(engine->context(), *mValue, nullptr, &byteLen, nullptr));
        if (buffer.type() == Type::ArrayBuffer) {
            return byteLen;
        }
        break; }
    default:
        break;
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
    return spurv::makeError(fmt::format("Invalid type for toString {}", static_cast<int>(type())));
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

Result<std::vector<ScriptValue>> ScriptValue::toVector() const
{
    if (type() != Type::Array) {
        return spurv::makeError("Invalid type for toVector");
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
        if (next.type() == ScriptValue::Type::Null) {
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
    if (!(type() & Type::Object)) {
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
    if (!(type() & Type::Object)) {
        return spurv::makeError("Invalid type for toMap");
    }

    std::unordered_map<std::string, ScriptValue> ret;
    forEachProperty(*this, [&ret](std::string &&name, ScriptValue &&value) -> void {
        ret[std::move(name)] = std::move(value);
    });
    return ret;
}

} // namespace spurv
