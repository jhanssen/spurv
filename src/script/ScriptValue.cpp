#include "ScriptValue.h"
#include "ScriptEngine.h"

namespace spurv {
ScriptValue::~ScriptValue()
{
    if (mValue) {
        JS_FreeValue(ScriptEngine::scriptEngine()->context(), *mValue);
    }

    // JSValue result = JS_Eval(ctx, src, len, scriptName, JS_EVAL_TYPE_MODULE);
    // if (JS_IsException(result) || JS_IsError(ctx, result)) {
    //     printf("error evaluating script: %s\n", scriptName);
    //     js_std_dump_error(ctx, result, stderr);
    // }
    // JS_FreeValue(ctx, result);


}

ScriptValue ScriptValue::makeError(std::string /*message*/)
{
    return {};
}
} // namespace spurv

