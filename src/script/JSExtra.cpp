#include "JSExtra.h"

// ### this includes SharedArrayBuffer
bool JS_IsArrayBuffer(JSContext *ctx, JSValueConst obj)
{
    size_t p;
    return JS_GetArrayBuffer(ctx, &p, obj);
}

// ### this includes DataView
bool JS_IsTypedArray(JSContext *ctx, JSValueConst obj)
{
    JSValue val = JS_GetTypedArrayBuffer(ctx, obj, nullptr, nullptr, nullptr);
    if (JS_IsException(val)) {
        return false;
    }

    JS_FreeValue(ctx, val);
    return true;
}
