#include "ScriptEngine.h"
#include "JSExtra.h"
#include "Builtins.h"

#include <cassert>

#include <Logger.h>
#include <FS.h>
#include <Formatting.h>

namespace spurv {
thread_local ScriptEngine *ScriptEngine::tScriptEngine;
ScriptEngine::ScriptEngine(EventLoop *eventLoop, const std::filesystem::path &appPath)
    : mEventLoop(eventLoop), mRuntime(JS_NewRuntime()), mContext(JS_NewContext(mRuntime)), mAppPath(appPath)
{
    assert(!tScriptEngine);
    tScriptEngine = this;
#define ScriptAtom(atom)                        \
    mAtoms.atom = JS_NewAtom(mContext, #atom);
#include "ScriptAtomsInternal.h"
    FOREACH_SCRIPTATOM(ScriptAtom);
#undef ScriptAtom

    mGlobal = ScriptValue(JS_GetGlobalObject(mContext));
    mSpurv = ScriptValue(std::vector<std::pair<std::string, ScriptValue>>());
    mGlobal.setProperty("spurv", mSpurv.clone());

    bindSpurvFunction("log", &Builtins::log);
    bindSpurvFunction("setProcessHandler", &Builtins::setProcessHandler);
    bindSpurvFunction("utf8tostring", &Builtins::utf8tostring);
    bindSpurvFunction("utf16tostring", &Builtins::utf16tostring);
    bindSpurvFunction("utf16letostring", &Builtins::utf16letostring);
    bindSpurvFunction("utf16betostring", &Builtins::utf16betostring);
    bindSpurvFunction("utf32tostring", &Builtins::utf32tostring);
    bindSpurvFunction("stringtoutf8", &Builtins::stringtoutf8);
    bindSpurvFunction("stringtoutf16", &Builtins::stringtoutf16);
    bindSpurvFunction("stringtoutf16le", &Builtins::stringtoutf16le);
    bindSpurvFunction("stringtoutf16be", &Builtins::stringtoutf16be);
    bindSpurvFunction("stringtoutf32", &Builtins::stringtoutf32);
    bindSpurvFunction("setKeyEventHandler", &Builtins::setKeyEventHandler);
    bindGlobalFunction("setTimeout", std::bind(&ScriptEngine::setTimeout, this, std::placeholders::_1));
    bindGlobalFunction("setInterval", std::bind(&ScriptEngine::setInterval, this, std::placeholders::_1));
    bindGlobalFunction("clearTimeout", std::bind(&ScriptEngine::clearTimeout, this, std::placeholders::_1));
    bindGlobalFunction("clearInterval", std::bind(&ScriptEngine::clearTimeout, this, std::placeholders::_1));
    bindGlobalFunction("exit", std::bind(&ScriptEngine::exit, this, std::placeholders::_1));

    const std::filesystem::path file = mAppPath / "../src/typescript/dist/spurv.js";
    auto ret = eval(file);
    if (ret.hasError()) {
        spdlog::critical("{}", ret.error().message);
    }
}

ScriptEngine::~ScriptEngine()
{
    mTimers.clear();
    mFunctions.clear();
    mSpurv.clear();
    mGlobal.clear();
    mProcessHandler.clear();
    mKeyHandler.clear();

#define ScriptAtom(atom)                        \
    JS_FreeAtom(mContext, mAtoms.atom);
#include "ScriptAtomsInternal.h"
    FOREACH_SCRIPTATOM(ScriptAtom);
#undef ScriptAtom
    JS_RunGC(mRuntime);
    JS_FreeContext(mContext);
    JS_FreeRuntime(mRuntime);
    assert(tScriptEngine = this);
    tScriptEngine = nullptr;
}

void ScriptEngine::setProcessHandler(ScriptValue &&value)
{
    mProcessHandler = std::move(value);
}

void ScriptEngine::setKeyEventHandler(ScriptValue &&value)
{
    spdlog::error("Set key event handler", value.isFunction());
    mKeyHandler = std::move(value);
}

void ScriptEngine::onKey(int key, int scancode, int action, int mods)
{
    if (!mKeyHandler.isFunction()) {
        spdlog::error("Got key but no key handler key: {} scancode: {} action: {} mods: {}\n",
                      key, scancode, action, mods);
        return;
    }
    ScriptValue object(ScriptValue::Object);
    object.setProperty("key", ScriptValue(key));
    object.setProperty("scancode", ScriptValue(scancode));
    object.setProperty("action", ScriptValue(action));
    object.setProperty("mods", ScriptValue(mods));
    mKeyHandler.call(object);
}

Result<void> ScriptEngine::eval(const std::filesystem::path &file)
{
    bool ok;
    std::string contents = readFile(file, &ok);
    if (!ok) {
        return makeError(fmt::format("Unable to read file {}", file));
    }
    return eval(file.string(), contents);
}

Result<void> ScriptEngine::eval(const std::string &url, const std::string &source)
{
    ScriptValue ret(JS_Eval(mContext, source.c_str(), source.size(), url.c_str(), JS_EVAL_TYPE_GLOBAL));

    if (ret.isException()) {
        ScriptValue exception(JS_GetException(mContext));
        ScriptValue stack = exception.getProperty("stack");
        return makeError(fmt::format("Failed to eval {}\n{}\n{}",
                                     url, exception.toString(), stack.toString()));
    }
    return {};
}

ScriptValue ScriptEngine::bindFunction(ScriptValue::Function &&function)
{
    const int magic = ++mMagic;
    std::unique_ptr<FunctionData> data = std::make_unique<FunctionData>();
    data->value = ScriptValue(JS_NewCFunctionData(mContext, bindHelper, 0, magic, 0, nullptr));
    data->function = std::move(function);
    ScriptValue ret = data->value.clone();
    mFunctions[magic] = std::move(data);
    return ret;
}

void ScriptEngine::bindSpurvFunction(const std::string &name, ScriptValue::Function &&function)
{
    ScriptValue func = bindFunction(std::move(function));
    mSpurv.setProperty(name, std::move(func));
}

void ScriptEngine::bindGlobalFunction(const std::string &name, ScriptValue::Function &&function)
{
    ScriptValue func = bindFunction(std::move(function));
    mGlobal.setProperty(name, std::move(func));
}

EventEmitter<void(int)>& ScriptEngine::onExit()
{
    return mOnExit;
}

JSValue ScriptEngine::bindHelper(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic, JSValue *)
{
    ScriptEngine *that = ScriptEngine::scriptEngine();
    auto it = that->mFunctions.find(magic);
    if (it == that->mFunctions.end()) {
        return JS_ThrowTypeError(ctx, "Function can't be found");
    }

    std::vector<ScriptValue> args(argc);
    for (int i=0; i<argc; ++i) {
        args[i] = ScriptValue(JS_DupValue(that->mContext, argv[i]));
    }

    ScriptValue ret = it->second->function(std::move(args));
    if (ret) {
        if (ret.isError()) {
            return JS_Throw(that->mContext, ret.leakValue());
        }
        return ret.leakValue();
    }
    return *ScriptValue::undefined();
}

ScriptValue ScriptEngine::setTimeoutImpl(EventLoop::TimerMode mode, std::vector<ScriptValue> &&args)
{
    if (args.empty() || !args[0].isFunction()) {
        return ScriptValue::makeError("First argument must be a function");
    }

    unsigned int ms = 0;
    if (args.size() > 1) {
        auto ret = args[1].toUint();
        if (ret.ok()) {
            ms = *ret;
        }
    }
    ScriptValue callback = std::move(args[0]);
    args.erase(args.begin(), args.begin() + 2);

    std::unique_ptr<TimerData> timerData = std::make_unique<TimerData>();
    timerData->timerMode = mode;
    timerData->args = std::move(args);
    timerData->callback = std::move(callback);
    const uint32_t id = mEventLoop->startTimer([this](uint32_t timerId) {
        const auto it = mTimers.find(timerId);
        if (it == mTimers.end()) {
            spdlog::error("Couldn't find timer with id {}", timerId);
            return;
        }
        auto data = std::move(it->second);
        mTimers.erase(it);
        data->callback.call(data->args);
    }, ms, mode);
    mTimers[id] = std::move(timerData);
    return ScriptValue(id);
}

ScriptValue ScriptEngine::setTimeout(std::vector<ScriptValue> &&args)
{
    return setTimeoutImpl(EventLoop::TimerMode::SingleShot, std::move(args));
}

ScriptValue ScriptEngine::setInterval(std::vector<ScriptValue> &&args)
{
    return setTimeoutImpl(EventLoop::TimerMode::Repeat, std::move(args));
}

ScriptValue ScriptEngine::clearTimeout(std::vector<ScriptValue> &&args)
{
    if (!args.empty()) {
        const auto id = args[0].toUint();
        if (id.ok()) {
            auto it = mTimers.find(*id);
            if (it != mTimers.end()) {
                mTimers.erase(it);
                mEventLoop->stopTimer(*id);
            }
        }
    }
    return ScriptValue();
}

ScriptValue ScriptEngine::exit(std::vector<ScriptValue> &&args)
{
    int exitCode = 0;
    if (args.size() > 0) {
        auto val = args[0].toInt();
        if (val.ok()) {
            exitCode = *val;
        }
    }

    mOnExit.emit(exitCode);
    return {};
}
} // namespace spurv

