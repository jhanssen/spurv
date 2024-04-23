#pragma once

#include <memory>
#include <filesystem>
#include <quickjs.h>

#include "ScriptValue.h"
#include "ScriptAtoms.h"

namespace spurv {
class ScriptEngine
{
public:
    ScriptEngine();
    ~ScriptEngine();

    ScriptValue eval(const std::filesystem::path &file);
    ScriptValue eval(const std::string &url, const std::string &source);

    void setProcessHandler(JSValue value);

    static ScriptEngine *scriptEngine() { return tScriptEngine; }

    JSRuntime *runtime() const { return mRuntime; }
    JSContext *context() const { return mContext; }

    const ScriptAtoms &atoms() { return mAtoms; }
    // void bindFunction(const std::string &name, std::function<ScriptValue(std::vector<ScriptValue> &&args)> &&function);
private:
    thread_local static ScriptEngine *tScriptEngine;

    JSRuntime *mRuntime = nullptr;
    JSContext *mContext = nullptr;
    ScriptAtoms mAtoms;
    JSValue mGlobal {};
    ScriptValue mProcessHandler;
};
} // namespace spurv
