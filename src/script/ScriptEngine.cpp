#include "ScriptEngine.h"
#include "JSExtra.h"
#include "Builtins.h"

#include <cassert>

#include <Logger.h>
#include <FS.h>
#include <Formatting.h>

namespace spurv {
thread_local ScriptEngine *ScriptEngine::tScriptEngine;
ScriptEngine::ScriptEngine(const std::filesystem::path &appPath)
    : mRuntime(JS_NewRuntime()), mContext(JS_NewContext(mRuntime)), mAppPath(appPath)
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
    mGlobal.setProperty("spurv", mSpurv);

    bindFunction("log", &Builtins::log);
    bindFunction("setProcessHandler", &Builtins::setProcessHandler);
    bindFunction("utf8tostring", &Builtins::utf8tostring);
    bindFunction("utf16tostring", &Builtins::utf16tostring);
    bindFunction("utf16letostring", &Builtins::utf16letostring);
    bindFunction("utf16betostring", &Builtins::utf16betostring);
    bindFunction("utf32tostring", &Builtins::utf32tostring);
    bindFunction("stringtoutf8", &Builtins::stringtoutf8);
    bindFunction("stringtoutf16", &Builtins::stringtoutf16);
    bindFunction("stringtoutf16le", &Builtins::stringtoutf16le);
    bindFunction("stringtoutf16be", &Builtins::stringtoutf16be);
    bindFunction("stringtoutf32", &Builtins::stringtoutf32);
    bindFunction("setKeyEventHandler", &Builtins::setKeyEventHandler);

    const std::filesystem::path file = mAppPath / "../src/typescript/dist/spurv.js";
    auto ret = eval(file);
    if (ret.hasError()) {
        spdlog::critical("{}", ret.error().message);
    }
}

ScriptEngine::~ScriptEngine()
{
#define ScriptAtom(atom)                        \
    JS_FreeAtom(mContext, mAtoms.atom);
#include "ScriptAtomsInternal.h"
    FOREACH_SCRIPTATOM(ScriptAtom);
#undef ScriptAtom
    JS_FreeContext(mContext);
    JS_RunGC(mRuntime);
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
    auto ret = data->value.clone();
    mFunctions[magic] = std::move(data);
    return ret;
}

void ScriptEngine::bindFunction(const std::string &name, ScriptValue::Function &&function)
{
    ScriptValue func = bindFunction(std::move(function));
    JS_SetPropertyStr(mContext, *mSpurv, name.c_str(), *func);
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
        args[i] = ScriptValue(argv[i]);
        args[i].ref();
    }

    ScriptValue ret = it->second->function(std::move(args));
    if (ret) {
        if (ret.isError()) {
            return JS_Throw(that->mContext, ret.acquire());
        }
        return ret.acquire();
    }
    return *ScriptValue::undefined();
}
} // namespace spurv

