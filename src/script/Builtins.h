#pragma once

#include <vector>
#include <ScriptValue.h>

namespace spurv {
// declare function utf8tostring(buffer: ArrayBuffer): string;
ScriptValue utf8tostring(std::vector<ScriptValue> &&args);

// declare function utf16tostring(buffer: ArrayBuffer): string;
ScriptValue utf16tostring(std::vector<ScriptValue> &&args);
// declare function utf16letostring(buffer: ArrayBuffer): string;
ScriptValue utf16letostring(std::vector<ScriptValue> &&args);
// declare function utf16betostring(buffer: ArrayBuffer): string;
ScriptValue utf16betostring(std::vector<ScriptValue> &&args);

// declare function utf32tostring(buffer: ArrayBuffer): string;
ScriptValue utf32tostring(std::vector<ScriptValue> &&args);

// declare function stringtoutf8(str: string): ArrayBuffer;
ScriptValue stringtoutf8(std::vector<ScriptValue> &&args);

// declare function stringtoutf16(str: string): ArrayBuffer;
ScriptValue stringtoutf16(std::vector<ScriptValue> &&args);
// declare function stringtoutf16le(str: string): ArrayBuffer;
ScriptValue stringtoutf16le(std::vector<ScriptValue> &&args);
// declare function stringtoutf16be(str: string): ArrayBuffer;
ScriptValue stringtoutf16be(std::vector<ScriptValue> &&args);

// declare function stringtoutf32(str: string): ArrayBuffer;
ScriptValue stringtoutf32(std::vector<ScriptValue> &&args);

} // namespace spurv
