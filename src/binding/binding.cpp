#include <node_api.h>

static napi_value Init(napi_env env, napi_value exports)
{
    static_cast<void>(env);
    return exports;
}

NAPI_MODULE(spurv, Init)
