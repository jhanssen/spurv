#pragma once

#include <memory>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <quickjs.h>

#include "ScriptValue.h"
#include "ScriptAtoms.h"

namespace spurv {
class ScriptEngine
{
public:
    ScriptEngine(const std::filesystem::path &appPath);
    ~ScriptEngine();

    ScriptValue eval(const std::filesystem::path &file);
    ScriptValue eval(const std::string &url, const std::string &source);

    void setProcessHandler(JSValue value);

    static ScriptEngine *scriptEngine() { return tScriptEngine; }

    JSRuntime *runtime() const { return mRuntime; }
    JSContext *context() const { return mContext; }

    const ScriptAtoms &atoms() { return mAtoms; }
    // could do some nicer template stuff
    void bindFunction(const std::string &name, std::function<ScriptValue(std::vector<ScriptValue> &&args)> &&function);
private:
    static JSValue bindHelper(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *);

    thread_local static ScriptEngine *tScriptEngine;
    int mMagic { 0 };
    struct FunctionData {
        ScriptValue value; // Do we need this for it to stay alive?
        std::function<ScriptValue(std::vector<ScriptValue> &&args)> function;
    };

    std::unordered_map<int, FunctionData> mFunctions;

    JSRuntime *mRuntime = nullptr;
    JSContext *mContext = nullptr;
    const std::filesystem::path mAppPath;
    ScriptAtoms mAtoms;
    JSValue mGlobal {};
    ScriptValue mProcessHandler;
};
} // namespace spurv
