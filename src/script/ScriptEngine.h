#pragma once

#include <deque>
#include <memory>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <quickjs.h>
#include <EventLoop.h>
#include <EventEmitter.h>
#include <UnorderedDense.h>
#include <uv.h>

#include "ScriptValue.h"
#include "ScriptAtoms.h"
#include "ScriptBufferSource.h"
#include "ScriptClass.h"

namespace spurv {
enum class ProcessFlag {
    None = 0x00,
    StdinClosed = 0x01,
    Stdout = 0x02,
    Stderr = 0x04,
    Strings = 0x08,
    // these are not in js
    HadStdinFromOptions = 0x10,
    Synchronous = 0x20
};

template <>
struct IsEnumBitmask<ProcessFlag> {
    static constexpr bool enable = true;
};

class ScriptEngine
{
public:
    ScriptEngine(EventLoop *eventLoop, const std::filesystem::path &appPath);
    ~ScriptEngine();

    Result<void> eval(const std::filesystem::path &file);
    Result<void> eval(const std::string &url, const std::string &source);

    void onKey(int key, int scancode, int action, int mods, const std::optional<std::string> &keyName);

    static ScriptEngine *scriptEngine() { return tScriptEngine; }

    JSRuntime *runtime() const { return mRuntime; }
    JSContext *context() const { return mContext; }

    const ScriptAtoms &atoms() { return mAtoms; }
    const std::array<JSClassID, NumScriptBufferSources> &bufferSourceIds() const { return mBufferSourceIds; }
    ScriptValue bindFunction(ScriptValue::Function &&function);
    void bindSpurvFunction(JSAtom atom, ScriptValue::Function &&function);
    void bindGlobalFunction(JSAtom atom, ScriptValue::Function &&function);
    bool addClass(ScriptClass &&clazz);

    EventEmitter<void(int)>& onExit();

private:
    friend class ScriptValue;
    class CallScope
    {
    public:
        CallScope(ScriptEngine *e)
            : engine(e), context(e->mContext)
        {
            ++engine->mCallDepth;
        }

        ~CallScope()
        {
            assert(engine->mCallDepth > 0);
            if (!--engine->mCallDepth) {
                engine->executeMicrotasks();
            }
        }

        ScriptEngine *const engine;
        JSContext *context;
    };

    size_t mCallDepth { 0 };

    void initScriptBufferSourceIds();
    void executeMicrotasks();
    static std::filesystem::path findExecutable(std::filesystem::path exec);

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
    ScriptValue queueMicrotask(std::vector<ScriptValue> &&args);

    // exit(number?: number): void;
    ScriptValue exit(std::vector<ScriptValue> &&args);

    struct ProcessData;
    std::vector<std::unique_ptr<ProcessData>>::iterator findProcessByPid(int pid);
    static void onWriteFinished(uv_write_t *req, int status);
    static JSValue bindHelper(JSContext *ctx, JSValueConst functionVal, int argc, JSValueConst *argv, int magic, JSValue *);
    static JSValue constructHelper(JSContext *ctx, JSValueConst functionVal, int argc, JSValueConst *argv, int magic, JSValue *);
    static void onProcessExit(uv_process_t *proc, int64_t exit_status, int term_signal);
    static void finalizeClassInstance(JSRuntime *rt, JSValue val);
    static JSValue classGetter(JSContext *ctx, JSValueConst this_val, int magic);
    static JSValue classGetterSetterGetter(JSContext *ctx, JSValueConst this_val, int magic);
    static JSValue classGetterSetterSetter(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic);
    static JSValue classConstant(JSContext *ctx, JSValueConst this_val, int magic);
    static JSValue classStaticConstant(JSContext *ctx, JSValueConst this_val, int magic);
    static JSValue classMethod(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
    static JSValue classStaticMethod(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
    static JSValue queuedMicrotask(JSContext *ctx, int argc, JSValueConst *argv);
    static void sendOutputEvent(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf, const char *type);


    thread_local static ScriptEngine *tScriptEngine;
    EventLoop *mEventLoop;
    EventEmitter<void(int)> mOnExit;

    int mMagic { 0 };
    struct FunctionData {
        ScriptValue value;
        ScriptValue::Function function;
    };

    struct ProcessData {
        ~ProcessData();

        std::string executable;
        uv_stdio_container_t child_stdio[3];
        uv_process_options_s options = {};

        char *stdoutBuf { nullptr };
        size_t stdoutBufSize { 0 };
        char *stderrBuf { nullptr };
        size_t stderrBufSize { 0 };

        ProcessFlag flags { ProcessFlag::None };

        std::optional<uv_process_t> proc;
        std::optional<uv_pipe_t> stdinPipe;
        std::optional<uv_pipe_t> stdoutPipe;
        std::optional<uv_pipe_t> stderrPipe;
        struct Write {
            Write(std::string &&bytes);

            std::string data;
            uv_write_t req;
            uv_buf_t buffer;
        };
        std::deque<std::unique_ptr<Write>> stdinWrites;
    };
    std::vector<std::unique_ptr<ProcessData>> mProcesses;

    unordered_dense::map<int, std::unique_ptr<FunctionData>> mFunctions;

    struct TimerData {
        EventLoop::TimerMode timerMode;
        ScriptValue callback;
        std::vector<ScriptValue> args;
    };

    unordered_dense::map<uint32_t, std::unique_ptr<TimerData>> mTimers;

    struct ScriptClassData {
        std::string name;
        std::vector<JSCFunctionListEntry> protoProperties, staticProperties;
        struct Getter {
            std::string name;
            ScriptClass::Getter get;
        };
        std::vector<Getter> getters;
        struct GetterSetter {
            std::string name;
            ScriptClass::Getter get;
            ScriptClass::Setter set;
        };
        std::vector<GetterSetter> getterSetters;
        struct Constant {
            std::string name;
            ScriptValue value;
        };

        struct Method {
            std::string name;
            ScriptClass::Method call;
        };

        std::vector<Constant> constants;
        std::vector<Method> methods;

        ScriptValue prototype, constructor;

        ScriptClass::Constructor construct;
    };

    unordered_dense::map<JSClassID, std::unique_ptr<ScriptClassData>> mClasses;
    std::vector<std::unique_ptr<ScriptClassData::Constant>> mStaticClassConstants;
    struct StaticMethod {
        std::string name;
        ScriptClass::StaticMethod call;
    };
    std::vector<std::unique_ptr<StaticMethod>> mStaticClassMethods;
    unordered_dense::map<int, JSClassID> mConstructors;

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
