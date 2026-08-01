#include <node_api.h>

#include "bindings.h"

static napi_value Init(napi_env env, napi_value exports)
{
    return spurv::binding::initialize(env, exports);
}

NAPI_MODULE(spurv, Init)
