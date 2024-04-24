#pragma once

#include <string>
#include <quickjs.h>
#include <optional>
#include <vector>
#include <functional>
#include <EnumClassBitmask.h>
#include <Result.h>

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
    explicit ScriptValue(const char *str, size_t len);
    explicit ScriptValue(const std::u8string &str);
    explicit ScriptValue(const char8_t *str, size_t len);
    explicit ScriptValue(const std::vector<ScriptValue> &array);
    explicit ScriptValue(const std::vector<std::pair<std::string, ScriptValue>> &object);
    explicit ScriptValue(Function &&function);
    explicit ScriptValue(Tag tag);
    ~ScriptValue();

    void ref();

    ScriptValue call(const std::vector<ScriptValue> &args);
    ScriptValue call(const ScriptValue &arg);

    ScriptValue clone() const;
    JSValue acquire();
    enum class Type {
        Invalid           = 0x000000,
        Undefined         = 0x000001,
        Null              = 0x000002,
        String            = 0x000004,
        Boolean           = 0x000008,
        Number            = 0x000010,
        Double            = 0x000020 | Number,
        Int               = 0x000040 | Number,
        Error             = 0x000080, // not sure how these are different
        Exception         = 0x000100,
        Object            = 0x000200,
        Array             = 0x000400 | Object,
        ArrayBuffer       = 0x000800 | Object,
        TypedArray        = 0x001000 | Object,
        Symbol            = 0x002000,
        BigNum            = 0x004000,
        BigInt            = 0x008000 | BigNum,
        BigFloat          = 0x010000 | BigNum,
        BigDecimal        = 0x020000 | BigNum,
        Function          = 0x040000
    };

    bool isInvalid() const;
    bool isValid() const;
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

    ScriptValue &operator=(ScriptValue &&other);

    static ScriptValue makeError(std::string message);
    static ScriptValue makeArrayBuffer(const void *data, size_t byteLength);
    static ScriptValue undefined();
    static ScriptValue null();

    Type type() const;

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
    Result<std::vector<ScriptValue>> toVector() const;
    Result<std::vector<std::pair<std::string, ScriptValue>>> toObject() const;
    Result<std::unordered_map<std::string, ScriptValue>> toMap() const;

    void forEach(std::function<void(const ScriptValue &value, int idx)> function);

    ScriptValue getProperty(const std::string &name) const;
    ScriptValue getProperty(uint32_t value) const;
    Result<void> setProperty(const std::string &name, const ScriptValue &value);
    Result<void> setProperty(uint32_t idx, const ScriptValue &value);

private:
    std::optional<JSValue> mValue;
    mutable std::optional<Type> mType;
};


template<>
struct IsEnumBitmask<ScriptValue::Type> {
    static constexpr bool enable = true;
};


inline bool ScriptValue::isInvalid() const
{
    return type() == Type::Invalid;
}

inline bool ScriptValue::isValid() const
{
    return mValue.has_value();
}

inline bool ScriptValue::isNull() const
{
    return type() == Type::Null;
}

inline bool ScriptValue::isNullOrUndefined() const
{
    switch (type()) {
    case Type::Null:
    case Type::Undefined:
        return true;
    default:
        break;
    }
    return false;
}

inline bool ScriptValue::isString() const
{
    return type() == Type::String;
}

inline bool ScriptValue::isBoolean() const
{
    return type() == Type::Boolean;
}

inline bool ScriptValue::isNumber() const
{
    return type() & Type::Number;
}

inline bool ScriptValue::isDouble() const
{
    return type() == Type::Double;
}

inline bool ScriptValue::isInt() const
{
    return type() == Type::Int;
}

inline bool ScriptValue::isError() const
{
    return type() == Type::Error;
}

inline bool ScriptValue::isException() const
{
    return type() == Type::Exception;
}

inline bool ScriptValue::isExceptionOrError() const
{
    switch (type()) {
    case Type::Exception:
    case Type::Error:
        return true;
    default:
        break;
    }
    return false;
}

inline bool ScriptValue::isObject() const
{
    return type() & Type::Object;
}

inline bool ScriptValue::isStrictObject() const
{
    return type() == Type::Object;
}

inline bool ScriptValue::isArray() const
{
    return type() == Type::Array;
}

inline bool ScriptValue::isArrayBuffer() const
{
    return type() == Type::ArrayBuffer;
}

inline bool ScriptValue::isTypedArray() const
{
    return type() == Type::TypedArray;
}

inline bool ScriptValue::isSymbol() const
{
    return type() == Type::Symbol;
}

inline bool ScriptValue::isBigNum() const
{
    return type() & Type::BigNum;
}

inline bool ScriptValue::isBigInt() const
{
    return type() == Type::BigInt;
}

inline bool ScriptValue::isBigFloat() const
{
    return type() == Type::BigFloat;
}

inline bool ScriptValue::isBigDecimal() const
{
    return type() == Type::BigDecimal;
}

inline bool ScriptValue::isFunction() const
{
    return type() == Type::Function;
}
} // namespace spurv
