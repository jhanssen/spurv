#pragma once

#include <memory>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <quickjs.h>
#include <EventLoop.h>
#include <EventEmitter.h>

#include "ScriptValue.h"
#include "ScriptAtoms.h"
#include "ScriptBufferSource.h"

namespace spurv {
class ScriptEngine
{
public:
    ScriptEngine(EventLoop *eventLoop, const std::filesystem::path &appPath);
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
    const std::array<JSClassID, NumScriptBufferSources> &bufferSourceIds() const { return mBufferSourceIds; }
    ScriptValue bindFunction(ScriptValue::Function &&function);
    void bindSpurvFunction(const std::string &name, ScriptValue::Function &&function);
    void bindGlobalFunction(const std::string &name, ScriptValue::Function &&function);

    EventEmitter<void(int)>& onExit();

private:
    void initScriptBufferSourceIds();
    // setTimeout(callback: (...args: unknown[]) => void, ms: number, ...args: unknown[]): number;
    ScriptValue setTimeoutImpl(EventLoop::TimerMode mode, std::vector<ScriptValue> &&args);
    ScriptValue setTimeout(std::vector<ScriptValue> &&args);
    // setInterval(callback: (...args: unknown[]) => void, ms: number, ...args: unknown[]): number;
    ScriptValue setInterval(std::vector<ScriptValue> &&args);
    // clearTimeout(timeoutId: number): void;
    // clearInterval(intervalId: number): void;
    ScriptValue clearTimeout(std::vector<ScriptValue> &&args);
    // queueMicrotask(callback: () => void): void;
    ScriptValue exit(std::vector<ScriptValue> &&args);

    static JSValue bindHelper(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *);

    thread_local static ScriptEngine *tScriptEngine;
    EventLoop *mEventLoop;
    EventEmitter<void(int)> mOnExit;

    int mMagic { 0 };
    struct FunctionData {
        ScriptValue value;
        ScriptValue::Function function;
    };

    std::unordered_map<int, std::unique_ptr<FunctionData>> mFunctions;

    struct TimerData {
        EventLoop::TimerMode timerMode;
        ScriptValue callback;
        std::vector<ScriptValue> args;
    };

    std::unordered_map<uint32_t, std::unique_ptr<TimerData>> mTimers;

    JSRuntime *mRuntime = nullptr;
    JSContext *mContext = nullptr;
    const std::filesystem::path mAppPath;
    ScriptAtoms mAtoms;
    std::array<JSClassID, NumScriptBufferSources> mBufferSourceIds;
    ScriptValue mGlobal;
    ScriptValue mSpurv;
    ScriptValue mProcessHandler;
    ScriptValue mKeyHandler;
};
} // namespace spurv
