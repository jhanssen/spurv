#pragma once

#include <string>
#include <quickjs.h>
#include <optional>
#include <functional>
#include <EnumClassBitmask.h>
#include <Result.h>

namespace spurv {
class ScriptValue
{
public:
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
    ~ScriptValue();

    void ref();

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
        DataView          = 0x001000 | Object,
        TypedArray        = 0x002000 | Object,
        Symbol            = 0x004000,
        BigNum            = 0x008000,
        BigInt            = 0x010000 | BigNum,
        BigFloat          = 0x020000 | BigNum,
        BigDecimal        = 0x040000 | BigNum,
        Function          = 0x080000
    };

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

private:
    std::optional<JSValue> mValue;
    mutable std::optional<Type> mType;
};

template<>
struct IsEnumBitmask<ScriptValue::Type> {
    static constexpr bool enable = true;
};
} // namespace spurv
