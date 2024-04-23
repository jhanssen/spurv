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
    mGlobal = JS_GetGlobalObject(mContext);

#define ScriptAtom(atom)                        \
    mAtoms.atom = JS_NewAtom(mContext, #atom);
#include "ScriptAtomsInternal.h"
    FOREACH_SCRIPTATOM(ScriptAtom);
#undef ScriptAtom

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

    const std::filesystem::path file = mAppPath / "../src/typescript/dist/spurv.js";
    // if (!eval(file)) {
    //     spdlog::critical("Failed to eval file {}", file);
    // }
}

ScriptEngine::~ScriptEngine()
{
    JS_FreeAtom(mContext, mAtoms.length);
    JS_FreeValue(mContext, mGlobal);
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

bool ScriptEngine::eval(const std::filesystem::path &file)
{
    bool ok;
    std::string contents = readFile(file, &ok);
    if (!ok) {
        return false;
    }
    return eval(file.string(), contents);
}

bool ScriptEngine::eval(const std::string &url, const std::string &source)
{
    return ScriptValue(JS_Eval(mContext, source.c_str(), source.size(), url.c_str(), JS_EVAL_TYPE_MODULE)).type() != ScriptValue::Type::Error;
}

void ScriptEngine::bindFunction(const std::string &name, std::function<ScriptValue(std::vector<ScriptValue> &&args)> &&function)
{
    const int magic = ++mMagic;
    std::unique_ptr<FunctionData> data = std::make_unique<FunctionData>();
    data->value = ScriptValue(JS_NewCFunctionData(mContext, bindHelper, 0, magic, 0, nullptr));
    data->function = std::move(function);
    JS_SetPropertyStr(mContext, mGlobal, name.c_str(), *data->value);
    mFunctions[magic] = std::move(data);
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
    }

    ScriptValue ret = it->second->function(std::move(args));
    if (ret) {
        return ret.acquire();
    }
    return ScriptValue::undefined().acquire();
}
} // namespace spurv
