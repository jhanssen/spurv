#pragma once

#include <quickjs.h>

extern bool JS_IsArrayBuffer(JSContext *ctx, JSValueConst obj);
extern bool JS_IsTypedArray(JSContext *ctx, JSValueConst obj);
