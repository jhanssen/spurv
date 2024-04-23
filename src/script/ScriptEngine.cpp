#include "ScriptEngine.h"
#include "JSExtra.h"

#include <cassert>

#include <Logger.h>
#include <FS.h>
#include <Formatting.h>

namespace spurv {
namespace {
JSValue log(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 2 || !JS_IsNumber(argv[0]) || !JS_IsString(argv[1])) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    uint32_t level;
    JS_ToUint32(ctx, &level, argv[0]);
    if (level < spdlog::level::trace || level > spdlog::level::critical) {
        return JS_ThrowTypeError(ctx, "Invalid level %u", level);
    }


    std::size_t len;
    const char *ret = JS_ToCStringLen(ctx, &len, argv[1]);

    spdlog::log({}, static_cast<spdlog::level::level_enum>(level), "{}", ret);
    return { {}, JS_TAG_UNDEFINED };
}

// declare function setProcessHandler(handler: (event: NativeProcessFinishedEvent | NativeProcessStdoutEvent | NativeProcessStderrEvent) => void): void;
JSValue setProcessHandler(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }
    ScriptEngine::scriptEngine()->setProcessHandler(argv[0]);
    return { {}, JS_TAG_UNDEFINED };
}

// declare function startProcess(arguments: string[], stdin: ArrayBuffer | boolean, stdout: boolean, stderr: boolean): number;
JSValue startProcess(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 4 || !JS_IsArray(ctx, argv[0]) || (!JS_IsBool(argv[1]) && !JS_IsArrayBuffer(ctx, argv[1]))
        || JS_IsBool(argv[2]) || JS_IsBool(argv[3])) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }
    ScriptEngine::scriptEngine()->setProcessHandler(argv[0]);
    return { {}, JS_TAG_UNDEFINED };
}

// declare function execProcess(arguments: string[], stdin: ArrayBuffer | boolean, stdout: boolean, stderr: boolean): NativeSynchronousProcessResult;
JSValue execProcess(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }
    ScriptEngine::scriptEngine()->setProcessHandler(argv[0]);
    return { {}, JS_TAG_UNDEFINED };
}

// declare function writeToProcessStdin(id: number, data: ArrayBuffer | string): void;
JSValue writeToProcessStdin(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }
    ScriptEngine::scriptEngine()->setProcessHandler(argv[0]);
    return { {}, JS_TAG_UNDEFINED };
}

// declare function closeProcessStdin(id: number): void;
JSValue closeProcessStdin(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }
    ScriptEngine::scriptEngine()->setProcessHandler(argv[0]);
    return { {}, JS_TAG_UNDEFINED };
}

} // anonymous namespace

thread_local ScriptEngine *ScriptEngine::tScriptEngine;
ScriptEngine::ScriptEngine(const std::filesystem::path &appPath)
    : mRuntime(JS_NewRuntime()), mContext(JS_NewContext(mRuntime)), mAppPath(appPath)
{
    mGlobal = JS_GetGlobalObject(mContext);

#define ScriptAtom(atom)                        \
    mAtoms.atom = JS_NewAtom(mContext, #atom);
#include "ScriptAtomsInternal.h"
        FOREACH_SCRIPTATOM(ScriptAtom)
#undef ScriptAtom

    JS_SetPropertyStr(mContext, mGlobal, "log", JS_NewCFunction(mContext, log, "log", 2));
    JS_SetPropertyStr(mContext, mGlobal, "setProcessHandler", JS_NewCFunction(mContext, spurv::setProcessHandler, "setProcessHandler", 1));
    JS_SetPropertyStr(mContext, mGlobal, "startProcess", JS_NewCFunction(mContext, startProcess, "startProcess", 4));
    JS_SetPropertyStr(mContext, mGlobal, "execProcess", JS_NewCFunction(mContext, execProcess, "execProcess", 4));
    JS_SetPropertyStr(mContext, mGlobal, "writeToProcessStdin", JS_NewCFunction(mContext, writeToProcessStdin, "writeToProcessStdin", 2));
    JS_SetPropertyStr(mContext, mGlobal, "closeProcessStdin", JS_NewCFunction(mContext, closeProcessStdin, "closeProcessStdin", 1));

    assert(!tScriptEngine);
    tScriptEngine = this;
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

void ScriptEngine::setProcessHandler(JSValue value)
{
    mProcessHandler = ScriptValue(value);
}

ScriptValue ScriptEngine::eval(const std::filesystem::path &file)
{
    bool ok;
    std::string contents = readFile(file, &ok);
    if (!ok) {
        return ScriptValue::makeError(fmt::format("Failed to open file {}", file));
    }
    return eval(file.string(), contents);
}

ScriptValue ScriptEngine::eval(const std::string &url, const std::string &source)
{
    return ScriptValue(JS_Eval(mContext, source.c_str(), source.size(), url.c_str(), JS_EVAL_TYPE_MODULE));
}

void ScriptEngine::bindFunction(const std::string &name, std::function<ScriptValue(std::vector<ScriptValue> &&args)> &&function)
{
    const int magic = ++mMagic;
    FunctionData data = {
        ScriptValue(JS_NewCFunctionData(mContext, bindHelper, 0, magic, 0, nullptr)),
        std::move(function)
    };
    JS_SetPropertyStr(mContext, mGlobal, name.c_str(), *data.value);
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

    ScriptValue ret = it->second.function(std::move(args));
    if (ret) {
        return *ret;
    }
    return *ScriptValue::undefined();
}
} // namespace spurv
