#include "ScriptValue.h"
#include "ScriptEngine.h"
#include "JSExtra.h"
#include <cassert>

namespace spurv {
ScriptValue::ScriptValue(JSValue value)
    : mValue(value)
{}

ScriptValue::~ScriptValue()
{
    if (mValue) {
        JS_FreeValue(ScriptEngine::scriptEngine()->context(), *mValue);
    }
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

ScriptValue ScriptValue::makeError(std::string /*message*/)
{
    return {};
}

JSValue ScriptValue::operator*()
{
    return *mValue;
}

bool ScriptValue::operator!() const
{
    return !mValue.has_value();
}

void ScriptValue::forEach(std::function<void(const ScriptValue &value, int idx)> function)
{
    ScriptEngine *engine = ScriptEngine::scriptEngine();
    auto ctx = engine->context();
    ScriptValue length = JS_GetProperty(ctx, *mValue, engine->atoms().length);
    int len;
    JS_ToInt32(ctx, &len, *length);
    for (int i = 0; i < len; ++i) {
        ScriptValue item = JS_GetPropertyUint32(ctx, *mValue, i);
        function(item, i);
    }
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

unsigned char *ScriptValue::arrayBufferData(size_t *length, bool *ok) const
{
    switch (type()) {
    case Type::ArrayBuffer: {
        size_t p;
        unsigned char *ret = JS_GetArrayBuffer(ScriptEngine::scriptEngine()->context(), &p, *mValue);
        if (ok)
            *ok = true;
        if (length)
            *length = p;
        return ret; }
    case Type::TypedArray: {
        auto context = ScriptEngine::scriptEngine()->context();
        size_t byteOffset, byteLen;
        ScriptValue buffer = JS_GetTypedArrayBuffer(context, *mValue, &byteOffset, &byteLen, nullptr);
        size_t p;
        unsigned char *ret = JS_GetArrayBuffer(context, &p, *mValue);
        if (ret) {
            if (ok)
                *ok = true;
            if (length)
                *length = byteLen;
            return ret + byteOffset;
        }
        if (ok)
            *ok = false;
        if (length)
            *length = 0;
        return nullptr; }
    default:
        break;
    }
    if (ok)
        *ok = false;
    if (length)
        *length = 0;
    return nullptr;
}

ScriptValue ScriptValue::typedArrayBuffer(bool *ok) const
{
    auto context = ScriptEngine::scriptEngine()->context();
    ScriptValue buffer = JS_GetTypedArrayBuffer(context, *mValue, nullptr, nullptr, nullptr);
    if (buffer.type() == Type::ArrayBuffer) {
        if (ok)
            *ok = true;
        return buffer;
    }

    if (ok)
        *ok = false;
    return ScriptValue();
}

size_t ScriptValue::length(bool *ok) const
{
    ScriptEngine *engine = ScriptEngine::scriptEngine();
    switch (type()) {
    case Type::ArrayBuffer: {
        size_t p;
        if (JS_GetArrayBuffer(engine->context(), &p, *mValue)) {
            if (ok)
                *ok = true;
            return p;
        }
        break; }
    case Type::Array:
    case Type::String: {
        ScriptValue length = JS_GetProperty(engine->context(), *mValue, engine->atoms().length);
        int len;
        JS_ToInt32(engine->context(), &len, *length);
        if (ok)
            *ok = true;
        return len; }
    case Type::TypedArray: {
        size_t byteLen;
        ScriptValue buffer = JS_GetTypedArrayBuffer(engine->context(), *mValue, nullptr, &byteLen, nullptr);
        if (buffer.type() == Type::ArrayBuffer) {
            if (ok)
                *ok = true;
            return byteLen;
        }
        break; }
    default:
        break;
    }
    if (ok)
        *ok = false;
    return 0;
}

bool ScriptValue::toBool(bool *ok) const
{
    if (mValue) {
        if (ok)
            *ok = true;
        // ### this will cast to bool, is that right?
        return JS_ToBool(ScriptEngine::scriptEngine()->context(), *mValue);
    }
    if (ok)
        *ok = false;
    return false;
}

std::string ScriptValue::toString(bool *ok) const
{
    if (mValue) {
        if (ok)
            *ok = true;
        auto context = ScriptEngine::scriptEngine()->context();
        size_t len;
        const char *str = JS_ToCStringLen(context, &len, *mValue);
        if (!str) {
            if (ok)
                *ok = false;
            return std::string();
        }
        std::string ret(str, len);
        JS_FreeCString(context, str);
        if (ok)
            *ok = true;
        return ret;
    }
    if (ok)
        *ok = false;
    return std::string();
}

double ScriptValue::toDouble(bool *ok) const
{
    if (mValue) {
        double ret;
        if (!JS_ToFloat64(ScriptEngine::scriptEngine()->context(), &ret, *mValue)) {
            if (ok)
                *ok = true;
            return ret;
        }
    }
    if (ok)
        *ok = false;
    return 0;
}

int32_t ScriptValue::toInt(bool *ok) const
{
    if (mValue) {
        int32_t ret;
        if (!JS_ToInt32(ScriptEngine::scriptEngine()->context(), &ret, *mValue)) {
            if (ok)
                *ok = true;
            return ret;
        }
    }
    if (ok)
        *ok = false;
    return 0;
}

uint32_t ScriptValue::toUint(bool *ok) const
{
    if (mValue) {
        uint32_t ret;
        if (!JS_ToUint32(ScriptEngine::scriptEngine()->context(), &ret, *mValue)) {
            if (ok)
                *ok = true;
            return ret;
        }
    }
    if (ok)
        *ok = false;
    return 0;
}

std::vector<ScriptValue> ScriptValue::toVector(bool *ok) const
{
    if (type() != Type::Array) {
        if (ok)
            *ok = false;
        return std::vector<ScriptValue>();
    }
    if (ok)
        *ok = true;

    auto engine = ScriptEngine::scriptEngine();
    auto ctx = engine->context();
    ScriptValue length = JS_GetProperty(ctx, *mValue, engine->atoms().length);
    int len;
    JS_ToInt32(ctx, &len, *length);
    std::vector<ScriptValue> ret(len);
    for (int i = 0; i < len; ++i) {
        ret[i] = JS_GetPropertyUint32(ctx, *mValue, i);
    }
    return ret;
}
} // namespace spurv

