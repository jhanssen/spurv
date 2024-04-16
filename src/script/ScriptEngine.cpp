#include "ScriptEngine.h"

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


    size_t len;
    const char *ret = JS_ToCStringLen(ctx, &len, argv[1]);

    spdlog::log({}, static_cast<spdlog::level::level_enum>(level), "{}", ret);
    return { {}, JS_TAG_UNDEFINED };
}
} // anonymous namespace

ScriptEngine::ScriptEngine()
    : mRuntime(JS_NewRuntime()), mContext(JS_NewContext(mRuntime))
{
    mGlobal = JS_GetGlobalObject(mContext);

    JS_SetPropertyStr(mContext, mGlobal, "log", JS_NewCFunction(mContext, log, "log", 2));

    assert(!tScriptEngine);
    tScriptEngine = this;
}

ScriptEngine::~ScriptEngine()
{
    JS_FreeValue(mContext, mGlobal);
    JS_FreeContext(mContext);
    JS_RunGC(mRuntime);
    JS_FreeRuntime(mRuntime);
    assert(tScriptEngine = this);
    tScriptEngine = nullptr;
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
    return JS_Eval(mContext, source.c_str(), source.size(), url.c_str(), JS_EVAL_TYPE_MODULE);
}
} // namespace spurv
