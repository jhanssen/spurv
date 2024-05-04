#pragma once

#include <string>
#include <quickjs.h>
#include <optional>
#include <vector>
#include <functional>
#include <EnumClassBitmask.h>
#include <Result.h>
#include <UnorderedDense.h>

namespace spurv {
class ScriptValue
{
public:
    using Function = std::function<ScriptValue(std::vector<ScriptValue> &&args)>;

    enum Tag {
        Object,
        Array
    };

    ScriptValue() = default;
    ScriptValue(ScriptValue &&other);
    explicit ScriptValue(JSValue value);
    explicit ScriptValue(bool value);
    explicit ScriptValue(int32_t value);
    explicit ScriptValue(uint32_t value);
    explicit ScriptValue(double value);
    explicit ScriptValue(const std::string &str);
    explicit ScriptValue(const char *str);
    explicit ScriptValue(const char *str, size_t len);
    explicit ScriptValue(const std::u8string &str);
    explicit ScriptValue(const char8_t *str, size_t len);
    explicit ScriptValue(std::vector<ScriptValue> &&array);
    explicit ScriptValue(std::vector<std::pair<std::string, ScriptValue>> &&object);
    explicit ScriptValue(std::vector<std::pair<JSAtom, ScriptValue>> &&object);
    explicit ScriptValue(Function &&function);
    explicit ScriptValue(Tag tag);
    ~ScriptValue();

    void ref();
    void clear();

    // should have variadic template
    ScriptValue call();
    ScriptValue call(const ScriptValue &arg);
    ScriptValue call(const std::vector<ScriptValue> &args);
    ScriptValue construct();
    ScriptValue construct(const ScriptValue &arg);
    ScriptValue construct(const std::vector<ScriptValue> &args);

    ScriptValue clone() const;
    JSValue leakValue();

    bool isInvalid() const;
    bool isValid() const;
    bool isUndefined() const;
    bool isNull() const;
    bool isNullOrUndefined() const;
    bool isString() const;
    bool isBoolean() const;
    bool isNumber() const;
    bool isDouble() const;
    bool isInt() const;
    bool isError() const;
    bool isException() const;
    bool isExceptionOrError() const;
    bool isObject() const;
    bool isStrictObject() const;
    bool isArray() const;
    bool isArrayBuffer() const;
    bool isTypedArray() const;
    bool isSymbol() const;
    bool isBigNum() const;
    bool isBigInt() const;
    bool isBigFloat() const;
    bool isBigDecimal() const;
    bool isFunction() const;
    bool isConstructor() const;

    std::string slowType() const;

    ScriptValue &operator=(ScriptValue &&other);

    static ScriptValue makeError(std::string message);
    static ScriptValue makeArrayBuffer(const void *data, size_t byteLength);
    static ScriptValue undefined();
    static ScriptValue null();

    bool operator!() const;
    operator bool() const;

    JSValue operator*() const;

    Result<ScriptValue> typedArrayBuffer() const;
    Result<std::pair<unsigned char *, std::size_t>> arrayBufferData() const;
    Result<std::size_t> length() const;
    Result<bool> toBool() const;
    Result<std::string> toString() const;
    Result<double> toDouble() const;
    Result<int32_t> toInt() const;
    Result<uint32_t> toUint() const;
    Result<std::vector<ScriptValue>> toArray() const;
    Result<std::vector<std::pair<std::string, ScriptValue>>> toObject() const;
    Result<unordered_dense::map<std::string, ScriptValue>> toMap() const;

    void forEach(std::function<void(const ScriptValue &value, int idx)> function);

    ScriptValue getProperty(JSAtom atom) const;
    ScriptValue getProperty(const std::string &name) const;
    ScriptValue getPropertyIdx(uint32_t value) const;
    Result<void> setProperty(JSAtom atom, ScriptValue &&value);
    Result<void> setProperty(const std::string &name, ScriptValue &&value);
    Result<void> setPropertyIdx(uint32_t idx, ScriptValue &&value);

private:
    std::optional<JSValue> mValue;
};

inline bool ScriptValue::isInvalid() const
{
    return !mValue;
}

inline bool ScriptValue::isValid() const
{
    return mValue.has_value();
}

inline bool ScriptValue::isNull() const
{
    return mValue && JS_IsNull(*mValue);
}

inline bool ScriptValue::isUndefined() const
{
    return mValue && JS_IsUndefined(*mValue);
}

inline bool ScriptValue::isNullOrUndefined() const
{
    return mValue && (JS_IsUndefined(*mValue) || JS_IsNull(*mValue));
}

inline bool ScriptValue::isString() const
{
    return mValue && JS_IsString(*mValue);
}

inline bool ScriptValue::isBoolean() const
{
    return mValue && JS_IsBool(*mValue);
}

inline bool ScriptValue::isNumber() const
{
    return mValue && JS_IsNumber(*mValue);
}

inline bool ScriptValue::isDouble() const
{
    return isNumber() && !isInt();
}

inline bool ScriptValue::isInt() const
{
    return mValue && JS_VALUE_GET_TAG(*mValue) == JS_TAG_INT;
}

inline bool ScriptValue::isException() const
{
    return mValue && JS_IsException(*mValue);
}

inline bool ScriptValue::isObject() const
{
    return mValue && JS_IsObject(*mValue);
}

inline bool ScriptValue::isSymbol() const
{
    return mValue && JS_IsSymbol(*mValue);
}

inline bool ScriptValue::isBigFloat() const
{
    return mValue && JS_IsBigFloat(*mValue);
}

inline bool ScriptValue::isBigDecimal() const
{
    return mValue && JS_IsBigDecimal(*mValue);
}

inline bool ScriptValue::isExceptionOrError() const
{
    return isException() || isError();
}

} // namespace spurv
