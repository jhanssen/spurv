#pragma once

namespace spurv {
enum class ScriptBufferSource : size_t  {
    ArrayBuffer = 0,
    DataView = 1,
    Int8Array = 2,
    Int16Array = 3,
    Int32Array = 4,
    BigInt64Array = 5,
    Uint8Array = 6,
    Uint8ClampedArray = 7,
    Uint16Array = 8,
    Uint32Array = 9,
    BigUint64Array = 10,
    Float32Array = 11,
    Float64Array = 12,
    Invalid = 13
};

static constexpr size_t NumScriptBufferSources = static_cast<size_t>(ScriptBufferSource::Invalid);

} // namespace spurv
