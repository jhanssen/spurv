#pragma once

#include <string>
#include <quickjs.h>
#include <optional>

namespace spurv {
class ScriptValue
{
public:
    ScriptValue() = default;
    ScriptValue(ScriptValue &&other) = default;
    ScriptValue(JSValue value);
    ~ScriptValue();

    ScriptValue &operator=(ScriptValue &&other) = default;

    static ScriptValue makeError(std::string message);

private:
    std::optional<JSValue> mValue;
};

} // namespace spurv
