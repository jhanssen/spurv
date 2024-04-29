#pragma once

#include <vector>
#include <ScriptValue.h>

namespace spurv {
namespace Builtins {
// log(level: LogLevel, message: string): void;
ScriptValue log(std::vector<ScriptValue> &&args);

// setProcessHandler(handler: (event: NativeProcessFinishedEvent | NativeProcessStdoutEvent | NativeProcessStderrEvent) => void): void;
ScriptValue setProcessHandler(std::vector<ScriptValue> &&args);

// utf8tostring(buffer: ArrayBuffer): string;
ScriptValue utf8tostring(std::vector<ScriptValue> &&args);

// utf16tostring(buffer: ArrayBuffer): string;
ScriptValue utf16tostring(std::vector<ScriptValue> &&args);
// utf16letostring(buffer: ArrayBuffer): string;
ScriptValue utf16letostring(std::vector<ScriptValue> &&args);
// utf16betostring(buffer: ArrayBuffer): string;
ScriptValue utf16betostring(std::vector<ScriptValue> &&args);

// utf32tostring(buffer: ArrayBuffer): string;
ScriptValue utf32tostring(std::vector<ScriptValue> &&args);

// stringtoutf8(str: string): ArrayBuffer;
ScriptValue stringtoutf8(std::vector<ScriptValue> &&args);

// stringtoutf16(str: string): ArrayBuffer;
ScriptValue stringtoutf16(std::vector<ScriptValue> &&args);
// stringtoutf16le(str: string): ArrayBuffer;
ScriptValue stringtoutf16le(std::vector<ScriptValue> &&args);
// stringtoutf16be(str: string): ArrayBuffer;
ScriptValue stringtoutf16be(std::vector<ScriptValue> &&args);

// stringtoutf32(str: string): ArrayBuffer;
ScriptValue stringtoutf32(std::vector<ScriptValue> &&args);

// setKeyEventHandler(handler: (event: KeyEvent) => void): void;
ScriptValue setKeyEventHandler(std::vector<ScriptValue> &&args);

// atob(str: string): string;
// atob(buf: ArrayBuffer): ArrayBuffer;S
ScriptValue atob(std::vector<ScriptValue> &&args);
// btoa(str: string): string;
// btoa(buf: ArrayBuffer): ArrayBuffer;
ScriptValue btoa(std::vector<ScriptValue> &&args);

} // namespace Builtins
} // namespace spurv
