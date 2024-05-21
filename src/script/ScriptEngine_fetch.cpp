#include "ScriptEngine.h"

namespace spurv {
ScriptValue ScriptEngine::fetchNative(std::vector<ScriptValue> &&args)
{
    if (args.empty() || !args[0].isObject()) {
        return ScriptValue::makeError("Invalid arguments");
    }

    auto url = args[0].getProperty("url").toString();
    if (!url.ok()) {
        return ScriptValue::makeError("Invalid url");
    }

    return ScriptValue::makeError("Not implemented yet");
}

ScriptValue ScriptEngine::setFetchNativeHandler(std::vector<ScriptValue> &&args)
{
    if (args.empty() || !args[0].isFunction()) {
        return ScriptValue::makeError("Invalid arguments");
    }

    mFetchNativeHandler = std::move(args[0]);
    return {};
}

ScriptValue ScriptEngine::abortFetchNative(std::vector<ScriptValue> &&/*args*/)
{
    return ScriptValue::makeError("Not implemented yet");
}
} // namespace spurv
