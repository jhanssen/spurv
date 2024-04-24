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

    Result<void> eval(const std::filesystem::path &file);
    Result<void> eval(const std::string &url, const std::string &source);

    void setProcessHandler(ScriptValue &&value);
    void setKeyEventHandler(ScriptValue &&value);

    void onKey(int key, int scancode, int action, int mods);

    static ScriptEngine *scriptEngine() { return tScriptEngine; }

    JSRuntime *runtime() const { return mRuntime; }
    JSContext *context() const { return mContext; }

    const ScriptAtoms &atoms() { return mAtoms; }
    ScriptValue bindFunction(ScriptValue::Function &&function);
    void bindFunction(const std::string &name, ScriptValue::Function &&function);
private:
    static JSValue bindHelper(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *);

    thread_local static ScriptEngine *tScriptEngine;
    int mMagic { 0 };
    struct FunctionData {
        ScriptValue value; // Do we need this for it to stay alive?
        ScriptValue::Function function;
    };

    std::unordered_map<int, std::unique_ptr<FunctionData>> mFunctions;

    JSRuntime *mRuntime = nullptr;
    JSContext *mContext = nullptr;
    const std::filesystem::path mAppPath;
    ScriptAtoms mAtoms;
    ScriptValue mGlobal;
    ScriptValue mSpurv;
    ScriptValue mProcessHandler;
    ScriptValue mKeyHandler;
};
} // namespace spurv
