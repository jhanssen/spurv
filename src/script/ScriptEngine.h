#pragma once

#include <memory>
#include <filesystem>
#include <quickjs.h>

#include "ScriptValue.h"

namespace spurv {
class ScriptEngine
{
public:
    ScriptEngine();
    ~ScriptEngine();

    ScriptValue eval(const std::filesystem::path &file);
    ScriptValue eval(const std::string &url, const std::string &source);

    static ScriptEngine *scriptEngine() { return tScriptEngine; }

    JSRuntime *runtime() const { return mRuntime; }
    JSContext *context() const { return mContext; }
private:
    thread_local static ScriptEngine *tScriptEngine;

    JSRuntime *mRuntime = nullptr;
    JSContext *mContext = nullptr;
    JSValue mGlobal {};
};
} // namespace spurv
