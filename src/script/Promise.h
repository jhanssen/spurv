#pragma once

#include <ScriptValue.h>

namespace spurv {
class Promise
{
public:
    Promise();
    ScriptValue value();
    void reject(const std::string &error);
    void resolve(ScriptValue &&value);
    void resolve();
private:
    ScriptValue mValue, mResolve, mReject;
};
} // namespace spurv
