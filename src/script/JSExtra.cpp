#include "JSExtra.h"
#include "ScriptEngine.h"
#include <cstddef>

void JS_FreeException(JSContext* ctx, JSValueConst obj)
{
    if (JS_IsUndefined(obj) || JS_IsNull(obj)) {
        return;
    }
    if (JS_IsObject(obj)) {
        const auto& atoms = spurv::ScriptEngine::scriptEngine()->atoms();
        auto message = JS_GetProperty(ctx, obj, atoms.message);
        if (!JS_IsUndefined(message)) {
            JS_FreeValue(ctx, message);
        }
    }
    JS_FreeValue(ctx, obj);
}

// ### this includes SharedArrayBuffer
bool JS_IsArrayBuffer(JSContext *ctx, JSValueConst obj)
{
    std::size_t p;
    const bool isab = JS_GetArrayBuffer(ctx, &p, obj);
    JS_FreeException(ctx, JS_GetException(ctx));
    return isab;
}

// ### this includes DataView
bool JS_IsTypedArray(JSContext *ctx, JSValueConst obj)
{
    JSValue val = JS_GetTypedArrayBuffer(ctx, obj, nullptr, nullptr, nullptr);
    if (JS_IsException(val)) {
        JS_FreeException(ctx, JS_GetException(ctx));
        return false;
    }

    JS_FreeValue(ctx, val);
    return true;
}
