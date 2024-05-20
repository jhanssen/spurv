#include "Promise.h"
#include "ScriptEngine.h"

namespace spurv {
Promise::Promise()
{
    auto context = ScriptEngine::scriptEngine()->context();
    JSValue resolvingFuncs[2];
    mValue = ScriptValue(JS_NewPromiseCapability(context, resolvingFuncs));
    mResolve = ScriptValue(resolvingFuncs[0]);
    mReject = ScriptValue(resolvingFuncs[1]);
}

ScriptValue Promise::value()
{
    return mValue.clone();
}

void Promise::reject(const std::string &error)
{
    mReject.call(ScriptValue::makeError(error));
}

void Promise::resolve()
{
    mResolve.call();
}

void Promise::resolve(ScriptValue &&value)
{
    mResolve.call(std::move(value));
}

} // namespace spurv
