#pragma once

#include <memory>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <quickjs.h>
#include <EventLoop.h>
#include <EventEmitter.h>
#include <uv.h>

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

    void onKey(int key, int scancode, int action, int mods);

    static ScriptEngine *scriptEngine() { return tScriptEngine; }

    JSRuntime *runtime() const { return mRuntime; }
    JSContext *context() const { return mContext; }

    const ScriptAtoms &atoms() { return mAtoms; }
    const std::array<JSClassID, NumScriptBufferSources> &bufferSourceIds() const { return mBufferSourceIds; }
    ScriptValue bindFunction(ScriptValue::Function &&function);
    void bindSpurvFunction(JSAtom atom, ScriptValue::Function &&function);
    void bindGlobalFunction(JSAtom atom, ScriptValue::Function &&function);

    EventEmitter<void(int)>& onExit();

private:
    void initScriptBufferSourceIds();

    // setProcessHandler(handler: (event: NativeProcessFinishedEvent | NativeProcessStdoutEvent | NativeProcessStderrEvent) => void): void;
    ScriptValue setProcessHandler(std::vector<ScriptValue> &&args);

    // startProcess(arguments: string[], env: Record<string, string> |
    //              undefined, cwd: string | undefined, stdin: ArrayBuffer | string |
    //              undefined, flags: number): number | string;
    ScriptValue startProcess(std::vector<ScriptValue> &&args);

    // execProcess(arguments: string[], env: Record<string, string> | undefined,
    //             cwd: string | undefined, stdin: ArrayBuffer | string | undefined, flags:
    //             number): NativeSynchronousProcessResult;
    ScriptValue execProcess(std::vector<ScriptValue> &&args);

    // writeToProcessStdin(pid: number, data: ArrayBuffer | string): void;
    ScriptValue writeToProcessStdin(std::vector<ScriptValue> &&args);
    // closeProcessStdin(pid: number): void;

    ScriptValue closeProcessStdin(std::vector<ScriptValue> &&args);
    // killProcess(pid: number): void;

    ScriptValue killProcess(std::vector<ScriptValue> &&args);
    // setKeyEventHandler(handler: (event: KeyEvent) => void): void;

    ScriptValue setKeyEventHandler(std::vector<ScriptValue> &&args);
    // setTimeout(callback: (...args: unknown[]) => void, ms: number, ...args: unknown[]): number;

    ScriptValue setTimeoutImpl(EventLoop::TimerMode mode, std::vector<ScriptValue> &&args);
    ScriptValue setTimeout(std::vector<ScriptValue> &&args);

    // setInterval(callback: (...args: unknown[]) => void, ms: number, ...args: unknown[]): number;
    ScriptValue setInterval(std::vector<ScriptValue> &&args);
    // clearTimeout(timeoutId: number): void;
    // clearInterval(intervalId: number): void;

    ScriptValue clearTimeout(std::vector<ScriptValue> &&args);

    // queueMicrotask(callback: () => void): void;
    // ScriptValue queueMicrotask(std::vector<ScriptValue> &&args);

    // exit(number?: number): void;
    ScriptValue exit(std::vector<ScriptValue> &&args);

    struct ProcessData;
    std::vector<std::unique_ptr<ProcessData>>::iterator findProcessByPid(int pid);
    static JSValue bindHelper(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *);
    static void onProcessExit(uv_process_t *proc, int64_t exit_status, int term_signal);

    thread_local static ScriptEngine *tScriptEngine;
    EventLoop *mEventLoop;
    EventEmitter<void(int)> mOnExit;

    int mMagic { 0 };
    struct FunctionData {
        ScriptValue value;
        ScriptValue::Function function;
    };

    struct ProcessData {
        uv_process_t proc;
        std::vector<unsigned char> pendingStdin;
    };
    std::vector<std::unique_ptr<ProcessData>> mProcesses;

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
