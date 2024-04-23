#include "Builtins.h"
#include <simdutf.h>

namespace spurv {
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

    if (!simdutf::validate_utf16(reinterpret_cast<const char16_t *>(data->first), data->second)) {
        return ScriptValue::makeError("Invalid utf16");
    }

    size_t len = simdutf::utf8_length_from_utf16(reinterpret_cast<const char16_t *>(data->first), data->second);
    std::string str;
    str.resize(len);
    const size_t converted = simdutf::convert_valid_utf16_to_utf8(reinterpret_cast<const char16_t *>(data->first), data->second, str.data());
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

    if (!simdutf::validate_utf16le(reinterpret_cast<const char16_t *>(data->first), data->second)) {
        return ScriptValue::makeError("Invalid utf16");
    }

    size_t len = simdutf::utf8_length_from_utf16le(reinterpret_cast<const char16_t *>(data->first), data->second);
    std::string str;
    str.resize(len);
    const size_t converted = simdutf::convert_valid_utf16le_to_utf8(reinterpret_cast<const char16_t *>(data->first), data->second, str.data());
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

    if (!simdutf::validate_utf16be(reinterpret_cast<const char16_t *>(data->first), data->second)) {
        return ScriptValue::makeError("Invalid utf16");
    }

    size_t len = simdutf::utf8_length_from_utf16be(reinterpret_cast<const char16_t *>(data->first), data->second);
    std::string str;
    str.resize(len);
    const size_t converted = simdutf::convert_valid_utf16be_to_utf8(reinterpret_cast<const char16_t *>(data->first), data->second, str.data());
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

    if (!simdutf::validate_utf32(reinterpret_cast<const char32_t *>(data->first), data->second)) {
        return ScriptValue::makeError("Invalid utf32");
    }

    size_t len = simdutf::utf8_length_from_utf32(reinterpret_cast<const char32_t *>(data->first), data->second);
    std::string str;
    str.resize(len);
    const size_t converted = simdutf::convert_valid_utf32_to_utf8(reinterpret_cast<const char32_t *>(data->first), data->second, str.data());
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
    return ScriptValue::makeArrayBuffer(vector.data(), converted);
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
    const size_t converted = simdutf::convert_valid_utf8_to_utf16le(data->c_str(), data->size(), vector.data());
    return ScriptValue::makeArrayBuffer(vector.data(), converted);
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
    return ScriptValue::makeArrayBuffer(vector.data(), converted);
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
    return ScriptValue::makeArrayBuffer(vector.data(), converted);
}
} // namespace spurv
