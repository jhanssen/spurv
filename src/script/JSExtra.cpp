#include "JSExtra.h"
#include <cstddef>

// ### this includes SharedArrayBuffer
bool JS_IsArrayBuffer(JSContext *ctx, JSValueConst obj)
{
    std::size_t p;
    const bool isab = JS_GetArrayBuffer(ctx, &p, obj);
    JS_GetException(ctx);
    return isab;
}

// ### this includes DataView
bool JS_IsTypedArray(JSContext *ctx, JSValueConst obj)
{
    JSValue val = JS_GetTypedArrayBuffer(ctx, obj, nullptr, nullptr, nullptr);
    if (JS_IsException(val)) {
        JS_GetException(ctx);
        return false;
    }

    JS_FreeValue(ctx, val);
    return true;
}
