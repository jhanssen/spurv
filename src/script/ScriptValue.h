#pragma once

#include <string>
#include <quickjs.h>
#include <optional>
#include <functional>

namespace spurv {
class ScriptValue
{
public:
    ScriptValue() = default;
    ScriptValue(ScriptValue &&other) = default;
    ScriptValue(JSValue value);
    ~ScriptValue();

    ScriptValue clone() const;
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
        BigDecimal        = 0x040000 | BigNum
    };

    ScriptValue &operator=(ScriptValue &&other) = default;

    static ScriptValue makeError(std::string message);

    Type type() const;

    bool operator!() const;

    JSValue operator*();

    ScriptValue typedArrayBuffer(bool *ok = nullptr) const;
    unsigned char *arrayBufferData(size_t *length = nullptr, bool *ok = nullptr) const;
    size_t length(bool *ok = nullptr) const;
    bool toBool(bool *ok = nullptr) const;
    std::string toString(bool *ok = nullptr) const;
    double toDouble(bool *ok = nullptr) const;
    int32_t toInt(bool *ok = nullptr) const;
    uint32_t toUint(bool *ok = nullptr) const;
    std::vector<ScriptValue> toVector(bool *ok = nullptr) const;

    void forEach(std::function<void(const ScriptValue &value, int idx)> function);

private:
    std::optional<JSValue> mValue;
    mutable std::optional<Type> mType;
};

} // namespace spurv
