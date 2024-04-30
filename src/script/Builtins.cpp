#include "Builtins.h"
#include "ScriptEngine.h"
#include <Logger.h>
#include <simdutf.h>
#include <base64.hpp>

namespace spurv {
namespace Builtins {
ScriptValue log(std::vector<ScriptValue> &&args)
{
    if (args.size() < 2 || !args[0].isNumber()) {
        return ScriptValue::makeError("Invalid arguments");
    }

    auto level = *args[0].toUint();
    if (level < spdlog::level::trace || level > spdlog::level::critical) {
        return ScriptValue::makeError(fmt::format("Invalid level {}", level));
    }

    auto str = args[1].toString();
    if (str.hasError()) {
        return ScriptValue::makeError(std::move(str).error().message);
    }

    spdlog::log({}, static_cast<spdlog::level::level_enum>(level), "{}", *str);
    return {};
}

ScriptValue utf8tostring(std::vector<ScriptValue> &&args)
{
    if (args.empty()) {
        return ScriptValue::makeError("Not enough args");
    }

    auto data = args[0].arrayBufferData();
    if (data.hasError()) {
        return ScriptValue::makeError(std::move(data).error().message);
    }

    if (!simdutf::validate_utf8(reinterpret_cast<const char *>(data->first), data->second)) {
        return ScriptValue::makeError("Invalid utf8");
    }

    return ScriptValue(reinterpret_cast<const char8_t *>(data->first), data->second);
}

ScriptValue utf16tostring(std::vector<ScriptValue> &&args)
{
    // ### we should make this more efficient, patch quickjs
    if (args.empty()) {
        return ScriptValue::makeError("Not enough args");
    }

    auto data = args[0].arrayBufferData();
    if (data.hasError()) {
        return ScriptValue::makeError(std::move(data).error().message);
    }

    if (!simdutf::validate_utf16(reinterpret_cast<const char16_t *>(data->first), data->second / sizeof(char16_t))) {
        return ScriptValue::makeError("Invalid utf16");
    }

    size_t len = simdutf::utf8_length_from_utf16(reinterpret_cast<const char16_t *>(data->first), data->second / sizeof(char16_t));
    std::string str;
    str.resize(len);
    const size_t converted = simdutf::convert_valid_utf16_to_utf8(reinterpret_cast<const char16_t *>(data->first), data->second / sizeof(char16_t), str.data());
    return ScriptValue(str.c_str(), converted);
}

ScriptValue utf16letostring(std::vector<ScriptValue> &&args)
{
    // ### we should make this more efficient, patch quickjs
    if (args.empty()) {
        return ScriptValue::makeError("Not enough args");
    }

    auto data = args[0].arrayBufferData();
    if (data.hasError()) {
        return ScriptValue::makeError(std::move(data).error().message);
    }

    if (!simdutf::validate_utf16le(reinterpret_cast<const char16_t *>(data->first), data->second / sizeof(char16_t))) {
        return ScriptValue::makeError("Invalid utf16");
    }

    size_t len = simdutf::utf8_length_from_utf16le(reinterpret_cast<const char16_t *>(data->first), data->second / sizeof(char16_t));
    std::string str;
    str.resize(len);
    const size_t converted = simdutf::convert_valid_utf16le_to_utf8(reinterpret_cast<const char16_t *>(data->first), data->second / sizeof(char16_t), str.data());
    return ScriptValue(str.c_str(), converted);
}

ScriptValue utf16betostring(std::vector<ScriptValue> &&args)
{
    // ### we should make this more efficient, patch quickjs
    if (args.empty()) {
        return ScriptValue::makeError("Not enough args");
    }

    auto data = args[0].arrayBufferData();
    if (data.hasError()) {
        return ScriptValue::makeError(std::move(data).error().message);
    }

    if (!simdutf::validate_utf16be(reinterpret_cast<const char16_t *>(data->first), data->second / sizeof(char16_t))) {
        return ScriptValue::makeError("Invalid utf16");
    }

    size_t len = simdutf::utf8_length_from_utf16be(reinterpret_cast<const char16_t *>(data->first), data->second / sizeof(char16_t));
    std::string str;
    str.resize(len);
    const size_t converted = simdutf::convert_valid_utf16be_to_utf8(reinterpret_cast<const char16_t *>(data->first), data->second / sizeof(char16_t), str.data());
    return ScriptValue(str.c_str(), converted);
}

ScriptValue utf32tostring(std::vector<ScriptValue> &&args)
{
    if (args.empty()) {
        return ScriptValue::makeError("Not enough args");
    }

    auto data = args[0].arrayBufferData();
    if (data.hasError()) {
        return ScriptValue::makeError(std::move(data).error().message);
    }

    if (!simdutf::validate_utf32(reinterpret_cast<const char32_t *>(data->first), data->second / sizeof(char32_t))) {
        return ScriptValue::makeError("Invalid utf32");
    }

    size_t len = simdutf::utf8_length_from_utf32(reinterpret_cast<const char32_t *>(data->first), data->second / sizeof(char32_t));
    std::string str;
    str.resize(len);
    const size_t converted = simdutf::convert_valid_utf32_to_utf8(reinterpret_cast<const char32_t *>(data->first), data->second / sizeof(char32_t), str.data());
    return ScriptValue(str.c_str(), converted);
}

ScriptValue stringtoutf8(std::vector<ScriptValue> &&args)
{
    if (args.empty()) {
        return ScriptValue::makeError("Not enough args");
    }

    auto data = args[0].toString();
    if (data.hasError()) {
        return ScriptValue::makeError(std::move(data).error().message);
    }

    return ScriptValue::makeArrayBuffer(reinterpret_cast<const unsigned char *>(data->c_str()), data->size());
}


ScriptValue stringtoutf16(std::vector<ScriptValue> &&args)
{
    if (args.empty()) {
        return ScriptValue::makeError("Not enough args");
    }

    auto data = args[0].toString();
    if (data.hasError()) {
        return ScriptValue::makeError(std::move(data).error().message);
    }

    size_t len = simdutf::utf16_length_from_utf8(data->c_str(), data->size());
    std::vector<char16_t> vector(len);
    const size_t converted = simdutf::convert_valid_utf8_to_utf16(data->c_str(), data->size(), vector.data());
    return ScriptValue::makeArrayBuffer(vector.data(), converted * sizeof(decltype(vector)::value_type));
}

ScriptValue stringtoutf16le(std::vector<ScriptValue> &&args)
{
    if (args.empty()) {
        return ScriptValue::makeError("Not enough args");
    }

    auto data = args[0].toString();
    if (data.hasError()) {
        return ScriptValue::makeError(std::move(data).error().message);
    }

    size_t len = simdutf::utf16_length_from_utf8(data->c_str(), data->size());
    std::vector<char16_t> vector(len);
    const size_t converted = simdutf::convert_valid_utf8_to_utf16le(data->c_str(), len, vector.data());
    return ScriptValue::makeArrayBuffer(vector.data(), converted * sizeof(decltype(vector)::value_type));
}

ScriptValue stringtoutf16be(std::vector<ScriptValue> &&args)
{
    if (args.empty()) {
        return ScriptValue::makeError("Not enough args");
    }

    auto data = args[0].toString();
    if (data.hasError()) {
        return ScriptValue::makeError(std::move(data).error().message);
    }

    size_t len = simdutf::utf16_length_from_utf8(data->c_str(), data->size());
    std::vector<char16_t> vector(len);
    const size_t converted = simdutf::convert_valid_utf8_to_utf16be(data->c_str(), data->size(), vector.data());
    return ScriptValue::makeArrayBuffer(vector.data(), converted * sizeof(decltype(vector)::value_type));
}

ScriptValue stringtoutf32(std::vector<ScriptValue> &&args)
{
    if (args.empty()) {
        return ScriptValue::makeError("Not enough args");
    }

    auto data = args[0].toString();
    if (data.hasError()) {
        return ScriptValue::makeError(std::move(data).error().message);
    }

    size_t len = simdutf::utf32_length_from_utf8(data->c_str(), data->size());
    std::vector<char32_t> vector(len);
    const size_t converted = simdutf::convert_valid_utf8_to_utf32(data->c_str(), data->size(), vector.data());
    return ScriptValue::makeArrayBuffer(vector.data(), converted * sizeof(decltype(vector)::value_type));
}

namespace {
enum class Mode {
    ArrayBuffer,
    String
};

std::string base64Decode(const void *buf, size_t len)
{
    const size_t rest = len % 4;
    if (!rest) {
        return base64::decode_into<std::string>(reinterpret_cast<const unsigned char *>(buf), reinterpret_cast<const unsigned char *>(buf) + len);
    }

    std::unique_ptr<unsigned char[]> buffer(new unsigned char[len + 4 - rest + 1]);
    memcpy(buffer.get(), buf, len);
    memset(buffer.get() + len, '=', 4 - rest);
    buffer[len + 4 - rest] = '\0';
    return base64::decode_into<std::string>(buffer.get(), buffer.get() + len + 4 - rest);
}
} // anonymous namespace
ScriptValue atob(std::vector<ScriptValue> &&args)
{
    std::optional<Mode> mode;
    if (args.size() > 1 && !args[1].isNullOrUndefined()) {
        auto m = args[1].toString();
        if (!m.ok()) {
            return ScriptValue::makeError("Invalid mode");
        }
        std::string mm = *m;
        if (mm == "string") {
            mode = Mode::String;
        } else if (mm == "ArrayBuffer") {
            mode = Mode::ArrayBuffer;
        } else {
            return ScriptValue::makeError("mode must be \"string\" or \"ArrayBuffer\"");
        }
    }

    std::string b;
    if (args[0].isArrayBuffer()) {
        auto data = args[0].arrayBufferData();
        if (!data.ok()) {
            return ScriptValue::makeError("Invalid ArrayBuffer");
        }
        try {
            b = base64Decode(data->first, data->second);
        } catch (const std::runtime_error &e) {
            return ScriptValue::makeError(e.what());
        }
        if (!mode) {
            mode = Mode::ArrayBuffer;
        }
    } else if (args[0].isString()) {
        auto data = args[0].toString();
        if (!data.ok()) {
            return ScriptValue::makeError("Invalid string");
        }
        try {
            b = base64Decode(data->c_str(), data->size());
        } catch (const std::runtime_error &e) {
            return ScriptValue::makeError(e.what());
        }
        if (!mode) {
            mode = Mode::String;
        }
    } else {
        return ScriptValue::makeError("Invalid arguments");
    }

    assert(mode);
    if (*mode == Mode::String) {
        return ScriptValue(b);
    }
    // ### we could make quickjs adopt this memory with relatively little work
    return ScriptValue::makeArrayBuffer(reinterpret_cast<const unsigned char *>(b.c_str()), b.size());
}

ScriptValue btoa(std::vector<ScriptValue> &&args)
{
    std::optional<Mode> mode;
    if (args.size() > 1 && !args[1].isNullOrUndefined()) {
        auto m = args[1].toString();
        if (!m.ok()) {
            return ScriptValue::makeError("Invalid mode");
        }
        std::string mm = *m;
        if (mm == "string") {
            mode = Mode::String;
        } else if (mm == "ArrayBuffer") {
            mode = Mode::ArrayBuffer;
        } else {
            return ScriptValue::makeError("mode must be \"string\" or \"ArrayBuffer\"");
        }
    }

    std::string a;
    if (args[0].isArrayBuffer()) {
        auto data = args[0].arrayBufferData();
        if (!data.ok()) {
            return ScriptValue::makeError("Invalid ArrayBuffer");
        }
        try {
            a = base64::encode_into<std::string>(data->first, data->first + data->second);
        } catch (const std::runtime_error &e) {
            return ScriptValue::makeError(e.what());
        }

        if (!mode) {
            mode = Mode::ArrayBuffer;
        }
    } else if (args[0].isString()) {
        auto data = args[0].toString();
        if (!data.ok()) {
            return ScriptValue::makeError("Invalid string");
        }
        try {
            a = base64::to_base64(*data);
        } catch (const std::runtime_error &e) {
            return ScriptValue::makeError(e.what());
        }
        if (!mode) {
            mode = Mode::String;
        }
    } else {
        return ScriptValue::makeError("Invalid arguments");
    }

    assert(mode);
    if (*mode == Mode::String) {
        return ScriptValue(a);
    }
    // ### we could make quickjs adopt this memory with relatively little work
    return ScriptValue::makeArrayBuffer(reinterpret_cast<const unsigned char *>(a.c_str()), a.size());
}
} // namespace Builtins
} // namespace spurv
