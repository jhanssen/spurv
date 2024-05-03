#include "ScriptEngine.h"
#include "Builtins.h"

#include <cassert>

#include <Logger.h>
#include <FS.h>
#include <Formatting.h>

namespace spurv {
thread_local ScriptEngine *ScriptEngine::tScriptEngine;
ScriptEngine::ScriptEngine(EventLoop *eventLoop, const std::filesystem::path &appPath)
    : mEventLoop(eventLoop), mRuntime(JS_NewRuntime()), mContext(JS_NewContext(mRuntime)), mAppPath(appPath)
{
    assert(!tScriptEngine);
    tScriptEngine = this;

    JS_SetMaxStackSize(mRuntime, 0);

#define ScriptAtom(atom)                        \
    mAtoms.atom = JS_NewAtom(mContext, #atom);
#include "ScriptAtomsInternal.h"
    FOREACH_SCRIPTATOM(ScriptAtom);
#undef ScriptAtom

    mGlobal = ScriptValue(JS_GetGlobalObject(mContext));
    initScriptBufferSourceIds();
    mSpurv = ScriptValue(std::vector<std::pair<std::string, ScriptValue>>());
    mGlobal.setProperty(mAtoms.spurv, mSpurv.clone());

    bindSpurvFunction(mAtoms.log, &Builtins::log);
    bindSpurvFunction(mAtoms.setProcessHandler, std::bind(&ScriptEngine::setProcessHandler, this, std::placeholders::_1));
    bindSpurvFunction(mAtoms.startProcess, std::bind(&ScriptEngine::startProcess, this, std::placeholders::_1));
    bindSpurvFunction(mAtoms.execProcess, std::bind(&ScriptEngine::execProcess, this, std::placeholders::_1));
    bindSpurvFunction(mAtoms.writeToProcessStdin, std::bind(&ScriptEngine::writeToProcessStdin, this, std::placeholders::_1));
    bindSpurvFunction(mAtoms.closeProcessStdin, std::bind(&ScriptEngine::closeProcessStdin, this, std::placeholders::_1));
    bindSpurvFunction(mAtoms.utf8tostring, &Builtins::utf8tostring);
    bindSpurvFunction(mAtoms.utf16tostring, &Builtins::utf16tostring);
    bindSpurvFunction(mAtoms.utf16letostring, &Builtins::utf16letostring);
    bindSpurvFunction(mAtoms.utf16betostring, &Builtins::utf16betostring);
    bindSpurvFunction(mAtoms.utf32tostring, &Builtins::utf32tostring);
    bindSpurvFunction(mAtoms.stringtoutf8, &Builtins::stringtoutf8);
    bindSpurvFunction(mAtoms.stringtoutf16, &Builtins::stringtoutf16);
    bindSpurvFunction(mAtoms.stringtoutf16le, &Builtins::stringtoutf16le);
    bindSpurvFunction(mAtoms.stringtoutf16be, &Builtins::stringtoutf16be);
    bindSpurvFunction(mAtoms.stringtoutf32, &Builtins::stringtoutf32);
    bindSpurvFunction(mAtoms.setKeyEventHandler, std::bind(&ScriptEngine::setKeyEventHandler, this, std::placeholders::_1));
    bindSpurvFunction(mAtoms.exit, std::bind(&ScriptEngine::exit, this, std::placeholders::_1));
    bindGlobalFunction(mAtoms.setTimeout, std::bind(&ScriptEngine::setTimeout, this, std::placeholders::_1));
    bindGlobalFunction(mAtoms.setInterval, std::bind(&ScriptEngine::setInterval, this, std::placeholders::_1));
    bindGlobalFunction(mAtoms.clearTimeout, std::bind(&ScriptEngine::clearTimeout, this, std::placeholders::_1));
    bindGlobalFunction(mAtoms.clearInterval, std::bind(&ScriptEngine::clearTimeout, this, std::placeholders::_1));
    bindGlobalFunction(mAtoms.atob, &Builtins::atob);
    bindGlobalFunction(mAtoms.btoa, &Builtins::btoa);

    const std::filesystem::path file = mAppPath / "../src/typescript/dist/spurv.js";
    auto ret = eval(file);
    if (ret.hasError()) {
        spdlog::critical("{}", ret.error().message);
    }
}

ScriptEngine::~ScriptEngine()
{
    mTimers.clear();
    mFunctions.clear();
    mSpurv.clear();
    mGlobal.clear();
    mProcessHandler.clear();
    mKeyHandler.clear();

#define ScriptAtom(atom)                        \
    JS_FreeAtom(mContext, mAtoms.atom);
#include "ScriptAtomsInternal.h"
    FOREACH_SCRIPTATOM(ScriptAtom);
#undef ScriptAtom
    JS_RunGC(mRuntime);
    JS_FreeContext(mContext);
    JS_FreeRuntime(mRuntime);
    assert(tScriptEngine = this);
    tScriptEngine = nullptr;
}

ScriptValue ScriptEngine::setProcessHandler(std::vector<ScriptValue> &&args)
{
    if (args.empty() || !args[0].isFunction()) {
        return ScriptValue::makeError("Invalid arguments");
    }

    mProcessHandler = std::move(args[0]);
    return {};
}

namespace {
void cleanupOptions(uv_process_options_s options)
{
    if (options.env) {
        for (size_t i=0; options.env[i]; ++i) {
            free(options.env[i]);
        }
        delete[] options.env;
    }

    if (options.args) {
        for (size_t i=0; options.args[i]; ++i) {
            free(options.args[i]);
        }
        delete[] options.args;
    }
}
// Duplicated in typescript
enum class ProcessFlag {
    None = 0x0,
    StdinClosed = 0x1,
    Stdout = 0x2,
    Stderr = 0x4,
    Strings = 0x8
};
} // anonymous namespace

template <>
struct IsEnumBitmask<ProcessFlag> {
    static constexpr bool enable = true;
};

ScriptValue ScriptEngine::startProcess(std::vector<ScriptValue> &&args)
{
    if (args.size() < 5 || !args[0].isArray() || !args[4].isNumber()) {
        return ScriptValue::makeError("Invalid arguments");
    }

    uv_loop_t *loop = static_cast<uv_loop_t *>(mEventLoop->handle());

    std::unique_ptr<ProcessData> data = std::make_unique<ProcessData>();

    ProcessFlag flags = static_cast<ProcessFlag>(*args[4].toInt());
    uv_stdio_container_t child_stdio[3];
    uv_process_options_s options = {};
    options.stdio = child_stdio;
    options.stdio_count = 3;

    if (flags & ProcessFlag::StdinClosed) {
        child_stdio[0].flags = UV_IGNORE;
    } else {
        // ### flag for blocking reads?
        child_stdio[0].flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_WRITABLE_PIPE | UV_NONBLOCK_PIPE);
        data->stdinPipe = {};
        uv_pipe_init(loop, &(*data->stdinPipe), 1);
    }

    if (!(flags & ProcessFlag::Stdout)) {
        child_stdio[1].flags = UV_IGNORE;
    } else {
        child_stdio[1].flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_READABLE_PIPE | UV_NONBLOCK_PIPE);
        data->stdoutPipe = {};
        uv_pipe_init(loop, &(*data->stdoutPipe), 1);
    }

    if (!(flags & ProcessFlag::Stderr)) {
        child_stdio[2].flags = UV_IGNORE;
    } else {
        child_stdio[2].flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_READABLE_PIPE | UV_NONBLOCK_PIPE);
        data->stderrPipe = {};
        uv_pipe_init(loop, &(*data->stderrPipe), 1);
    }

    if (!args[1].isNullOrUndefined()) {
        if (!args[1].isObject()) {
            return ScriptValue::makeError("Invalid env argument");
        }
        auto vec = *args[1].toObject();
        options.env = new char*[vec.size() + 1];
        options.env[vec.size()] = nullptr;
        size_t i = 0;
        for (std::pair<std::string, ScriptValue> &arg : vec) {
            auto str = arg.second.toString();
            if (!str.ok()) {
                cleanupOptions(options);
                return ScriptValue::makeError("Couldn't convert env argument with key " + arg.first);
            }
            options.env[i++] = strdup(fmt::format("{}={}", arg.first, str->c_str()).c_str());
        }
    }

    std::string cwd;
    if (!args[2].isNullOrUndefined()) {
        auto cwdArg = args[2].toString();
        if (cwdArg.ok()) {
            cwd = *cwdArg;
            options.cwd = cwd.c_str();
        } else {
            cleanupOptions(options);
            return ScriptValue::makeError("Invalid cwd argument");
        }
    }

    if (args[2].isArrayBuffer() || args[3].isTypedArray()) {
        auto arrayBufferData = *args[2].arrayBufferData(); // ### can this fail?
        data->pendingStdin.resize(arrayBufferData.second);
        memcpy(data->pendingStdin.data(), arrayBufferData.first, arrayBufferData.second);
    } else if (!args[2].isNullOrUndefined()) {
        auto str = args[2].toString();
        if (!str.ok()) {
            cleanupOptions(options);
            return ScriptValue::makeError("Invalid stdin argument");
        }
        const auto string = *str;
        data->pendingStdin.resize(string.size());
        memcpy(data->pendingStdin.data(), string.c_str(), string.size());
    }
    // ### Need to handle stdin

    const std::vector<ScriptValue> argv = *args[0].toArray();

    options.args = new char*[argv.size() + 1];
    memset(options.args, 0, sizeof(char *) * argv.size() + 1);

    for (size_t i=0; i<argv.size(); ++i) {
        auto str = argv[i].toString();
        if (!str.ok()) {
            cleanupOptions(options);
            return ScriptValue::makeError("Invalid arguments");
        }
        options.args[i] = strdup(str->c_str());
    }
    options.file = options.args[0];
    options.exit_cb = ScriptEngine::onProcessExit;

    data->proc = {};
    const int ret = uv_spawn(loop, &(*data->proc), &options);
    if (ret) {
        // failed to launch
        const char *err = uv_strerror(ret);
        assert(err);
        std::string error = fmt::format("Failed to launch {}: {}", options.file, err);
        cleanupOptions(options);
        return ScriptValue(error);
    }
    cleanupOptions(options);
    const int pid = data->proc->pid;
    // dummy_buf = uv_buf_init("a", 1);
    // struct child_worker *worker = &workers[round_robin_counter];
    // uv_write2(write_req, (uv_stream_t*) &worker->pipe, &dummy_buf, 1, (uv_stream_t*) client, NULL);

    mProcesses.push_back(std::move(data));
    return ScriptValue(pid);
}

ScriptValue ScriptEngine::execProcess(std::vector<ScriptValue> &&args)
{
    static_cast<void>(args);
    return {};
}

ScriptValue ScriptEngine::writeToProcessStdin(std::vector<ScriptValue> &&args)
{
    static_cast<void>(args);
    return {};
}

ScriptValue ScriptEngine::closeProcessStdin(std::vector<ScriptValue> &&args)
{
    static_cast<void>(args);
    return {};
}

ScriptValue ScriptEngine::killProcess(std::vector<ScriptValue> &&args)
{
    if (args.size() < 2 || !args[0].isInt() || !args[1].isInt()) {
        return ScriptValue::makeError("Invalid arguments");
    }

    auto it = findProcessByPid(*args[0].toInt());
    if (it == mProcesses.end()) {
        return ScriptValue::makeError("Can't find process");
    }


    uv_process_kill(&*(*it)->proc, *args[1].toInt());
    return {};
}

void ScriptEngine::onKey(int key, int scancode, int action, int mods)
{
    if (!mKeyHandler.isFunction()) {
        spdlog::error("Got key but no key handler key: {} scancode: {} action: {} mods: {}\n",
                      key, scancode, action, mods);
        return;
    }
    ScriptValue object(ScriptValue::Object);
    object.setProperty(mAtoms.key, ScriptValue(key));
    object.setProperty(mAtoms.scancode, ScriptValue(scancode));
    object.setProperty(mAtoms.action, ScriptValue(action));
    object.setProperty(mAtoms.mods, ScriptValue(mods));
    mKeyHandler.call(object);
}

Result<void> ScriptEngine::eval(const std::filesystem::path &file)
{
    bool ok;
    std::string contents = readFile(file, &ok);
    if (!ok) {
        return makeError(fmt::format("Unable to read file {}", file));
    }
    return eval(file.string(), contents);
}

Result<void> ScriptEngine::eval(const std::string &url, const std::string &source)
{
    ScriptValue ret(JS_Eval(mContext, source.c_str(), source.size(), url.c_str(), JS_EVAL_TYPE_GLOBAL));

    if (ret.isException()) {
        ScriptValue exception(JS_GetException(mContext));
        ScriptValue stack = exception.getProperty("stack");
        return makeError(fmt::format("Failed to eval {}\n{}\n{}",
                                     url, exception.toString(), stack.toString()));
    }
    return {};
}

ScriptValue ScriptEngine::bindFunction(ScriptValue::Function &&function)
{
    const int magic = ++mMagic;
    std::unique_ptr<FunctionData> data = std::make_unique<FunctionData>();
    data->value = ScriptValue(JS_NewCFunctionData(mContext, bindHelper, 0, magic, 0, nullptr));
    data->function = std::move(function);
    ScriptValue ret = data->value.clone();
    mFunctions[magic] = std::move(data);
    return ret;
}

void ScriptEngine::bindSpurvFunction(JSAtom atom, ScriptValue::Function &&function)
{
    ScriptValue func = bindFunction(std::move(function));
    mSpurv.setProperty(atom, std::move(func));
}

void ScriptEngine::bindGlobalFunction(JSAtom atom, ScriptValue::Function &&function)
{
    ScriptValue func = bindFunction(std::move(function));
    mGlobal.setProperty(atom, std::move(func));
}

bool ScriptEngine::addClass(ScriptClass &&clazz)
{
    std::unique_ptr<ScriptClassData> data = std::make_unique<ScriptClassData>();
    data->name = clazz.name();
    JSClassDef def = {
        data->name.c_str(),
        ScriptEngine::finalizeClassInstance,
        nullptr, // gc_mark,
        nullptr, // ScriptEngine::constructClassInstance,
        nullptr
    };

    JSClassID id;
    JS_NewClassID(&id);
    if (JS_NewClass(mRuntime, id, &def) != 0) {
        return false;
    }

    // const auto &ctor = data->clazz.constructor();
    auto &properties = clazz.properties();
    auto &methods = clazz.methods();
    std::vector<JSCFunctionListEntry> protoProperties(properties.size() + methods.size()); // ### does this have to survive?
    memset(protoProperties.data(), 0, protoProperties.size() * sizeof(JSCFunctionListEntry));
    size_t idx = 0;
    for (auto &prop : properties) {
        auto &protoProp = protoProperties[idx++];
        protoProp.name = prop.first.c_str();
        protoProp.prop_flags = JS_PROP_C_W_E;
        protoProp.def_type = JS_DEF_CGETSET_MAGIC;
        if (std::holds_alternative<ScriptValue>(prop.second)) {
            protoProp.prop_flags &= ~JS_PROP_WRITABLE;
            protoProp.u.getset.get.getter_magic = &ScriptEngine::classConstant;
            protoProp.magic = static_cast<int16_t>(ScriptClassData::FirstConstant + data->constants.size());
            data->constants.push_back(std::make_pair(std::move(prop.first), std::get<ScriptValue>(std::move(prop.second))));
        } else if (std::holds_alternative<ScriptClass::Getter>(prop.second)) {
            protoProp.prop_flags &= ~JS_PROP_WRITABLE;
            protoProp.u.getset.get.getter_magic = &ScriptEngine::classGetter;
            protoProp.magic = static_cast<int16_t>(ScriptClassData::FirstGetter + data->getters.size());
            data->getters.push_back(std::make_pair(std::move(prop.first), std::get<ScriptClass::Getter>(std::move(prop.second))));
        } else {
            protoProp.u.getset.get.getter_magic = &ScriptEngine::classGetter;
            protoProp.u.getset.set.setter_magic = &ScriptEngine::classGetterSetterSetter;
            protoProp.magic = static_cast<int16_t>(ScriptClassData::FirstGetterSetter + data->getterSetters.size());
            data->getterSetters.push_back(std::make_pair(std::move(prop.first), std::get<std::pair<ScriptClass::Getter, ScriptClass::Setter>>(std::move(prop.second))));
        }
    }

    data->methods.reserve(methods.size());
    for (auto &method : methods) {
        auto &protoProp = protoProperties[idx++];
        protoProp.name = method.first.c_str();
        protoProp.prop_flags = JS_PROP_C_W_E;
        protoProp.def_type = JS_DEF_CFUNC;
        protoProp.magic = static_cast<int16_t>(data->methods.size());
        auto &func = protoProp.u.func;
        func.length = 1;
        func.cproto = JS_CFUNC_generic_magic;
        func.cfunc.generic_magic = &ScriptEngine::classMethod;
    }
    assert(idx == protoProperties.size());

    JSValue proto = JS_NewObject(mContext);
    JS_SetPropertyFunctionList(mContext, proto, protoProperties.data(), protoProperties.size());

    const int magic = ++mMagic;
    JSValue constructor = JS_NewCFunctionData(mContext, constructHelper, 0, magic, 0, nullptr);
    JS_SetConstructor(mContext, constructor, proto);

    mConstructors[magic] = id;

    return true;
}

void ScriptEngine::initScriptBufferSourceIds()
{
    ScriptValue arrayBuffer(JS_NewArrayBuffer(mContext, nullptr, 0, nullptr, nullptr, false));
    mBufferSourceIds[static_cast<size_t>(ScriptBufferSource::ArrayBuffer)] = JS_GetClassID(*arrayBuffer);

    struct {
        const char *const name;
        const ScriptBufferSource idx;
    } const types[] = {
        { "DataView", ScriptBufferSource::DataView },
        { "Int16Array", ScriptBufferSource::Int16Array },
        { "Int8Array", ScriptBufferSource::Int8Array },
        { "BigInt64Array", ScriptBufferSource::BigInt64Array },
        { "Uint8Array", ScriptBufferSource::Uint8Array },
        { "Uint8ClampedArray", ScriptBufferSource::Uint8ClampedArray },
        { "Uint16Array", ScriptBufferSource::Uint16Array },
        { "Uint32Array", ScriptBufferSource::Uint32Array },
        { "BigUint64Array", ScriptBufferSource::BigUint64Array },
        { "Float32Array", ScriptBufferSource::Float32Array },
        { "Float64Array", ScriptBufferSource::Float64Array }
    };

    for (const auto &pair : types) {
        ScriptValue constructor = mGlobal.getProperty(pair.name);
        assert(constructor.isConstructor());
        ScriptValue instance = constructor.construct(arrayBuffer);
        assert(instance.isObject());
        mBufferSourceIds[static_cast<size_t>(pair.idx)] = JS_GetClassID(*instance);
    }
}

EventEmitter<void(int)>& ScriptEngine::onExit()
{
    return mOnExit;
}

std::vector<std::unique_ptr<ScriptEngine::ProcessData>>::iterator ScriptEngine::findProcessByPid(int pid)
{
    for (auto it = mProcesses.begin(); it != mProcesses.end(); ++it) {
        if ((*it)->proc->pid == pid) {
            return it;
        }
    }
    return mProcesses.end();
}

// static
JSValue ScriptEngine::bindHelper(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic, JSValue *)
{
    ScriptEngine *that = ScriptEngine::scriptEngine();
    auto it = that->mFunctions.find(magic);
    if (it == that->mFunctions.end()) {
        return JS_ThrowTypeError(ctx, "Function can't be found");
    }

    std::vector<ScriptValue> args(argc);
    for (int i=0; i<argc; ++i) {
        args[i] = ScriptValue(JS_DupValue(that->mContext, argv[i]));
    }

    ScriptValue ret = it->second->function(std::move(args));
    if (ret) {
        if (ret.isError()) {
            return JS_Throw(that->mContext, ret.leakValue());
        }
        return ret.leakValue();
    }
    return *ScriptValue::undefined();
}

// static
JSValue ScriptEngine::constructHelper(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *)
{
    ScriptEngine *that = ScriptEngine::scriptEngine();
    auto it = that->mConstructors.find(magic);
    if (it == that->mConstructors.end()) {
        return JS_ThrowTypeError(ctx, "Constructor can't be found (1)");
    }

    auto it2 = that->mClasses.find(it->second);
    if (it2 == that->mClasses.end()) {
        return JS_ThrowTypeError(ctx, "Constructor can't be found (2)");
    }

    std::vector<ScriptValue> args(argc);
    for (int i=0; i<argc; ++i) {
        args[i] = ScriptValue(JS_DupValue(that->mContext, argv[i]));
    }

    ScriptClassInstance *instance = nullptr;
    std::optional<std::string> ret = it2->second->constructor(instance, std::move(args));
    if (ret) {
        delete instance;
        return JS_ThrowTypeError(ctx, "Failed to construct class %s: %s", it2->second->name.c_str(), ret->c_str());
    }
    JS_SetOpaque(this_val, instance);
    return this_val;
}

// static
void ScriptEngine::onProcessExit(uv_process_t *proc, int64_t exit_status, int term_signal)
{
    ScriptEngine *that = ScriptEngine::scriptEngine();
    assert(that);
    for (auto it = that->mProcesses.begin(); it != that->mProcesses.end(); ++it) {
        if (&*(*it)->proc == proc) {
            if (that->mProcessHandler.isFunction()) {
                ScriptValue object(ScriptValue::Object);
                object.setProperty(that->mAtoms.type, ScriptValue("finished"));
                object.setProperty(that->mAtoms.pid, ScriptValue(proc->pid));
                assert(exit_status <= std::numeric_limits<int>::max() && exit_status >= std::numeric_limits<int>::min());
                // ### term_signal ?
                object.setProperty(that->mAtoms.exitCode, ScriptValue(static_cast<int>(exit_status)));
                that->mProcessHandler.call(object);
            }
            that->mProcesses.erase(it);
            return;
        }
    }
    // ### if this happens we've probably invalid'read'ed libuv already
    spdlog::error("Couldn't find process with pid {} for onProcessExit wiht exit_status {} and term_signal {}",
                  proc->pid, exit_status, term_signal);
    uv_close(reinterpret_cast<uv_handle_t *>(proc), nullptr);
}

// static
void ScriptEngine::finalizeClassInstance(JSRuntime *, JSValue val)
{
    JSClassID id = JS_GetClassID(val);
    void *opaque = JS_GetOpaque(val, id);
    if (opaque) {
        delete static_cast<ScriptClassInstance *>(opaque);
    }
}

// static
JSValue ScriptEngine::classGetter(JSContext *ctx, JSValueConst this_val, int magic)
{
    const JSClassID id = JS_GetClassID(this_val);
    ScriptEngine *const that = ScriptEngine::scriptEngine();
    assert(that);
    auto it = that->mClasses.find(id);
    if (it == that->mClasses.end()) {
        return JS_ThrowTypeError(ctx, "Class can't be found");
    }

    const size_t offset = magic - ScriptEngine::ScriptClassData::FirstGetter;
    if (offset >= it->second->getters.size()) {
        return JS_ThrowTypeError(ctx, "Property can't be found");
    }

    ScriptClassInstance *instance = static_cast<ScriptClassInstance *>(JS_GetOpaque(this_val, id));
    ScriptValue ret = it->second->getters[offset].second(instance);
    if (ret) {
        if (ret.isError()) {
            return JS_Throw(that->mContext, ret.leakValue());
        }
        return ret.leakValue();
    }
    return *ScriptValue::undefined();
}

// static
JSValue ScriptEngine::classGetterSetterGetter(JSContext *ctx, JSValueConst this_val, int magic)
{
    const JSClassID id = JS_GetClassID(this_val);
    ScriptEngine *const that = ScriptEngine::scriptEngine();
    assert(that);
    auto it = that->mClasses.find(id);
    if (it == that->mClasses.end()) {
        return JS_ThrowTypeError(ctx, "Class can't be found");
    }

    const size_t offset = magic - ScriptEngine::ScriptClassData::FirstGetterSetter;
    if (offset >= it->second->getterSetters.size()) {
        return JS_ThrowTypeError(ctx, "Property can't be found");
    }

    ScriptClassInstance *instance = static_cast<ScriptClassInstance *>(JS_GetOpaque(this_val, id));
    ScriptValue ret = it->second->getterSetters[offset].second.first(instance);
    if (ret) {
        if (ret.isError()) {
            return JS_Throw(that->mContext, ret.leakValue());
        }
        return ret.leakValue();
    }
    return *ScriptValue::undefined();
}

// static
JSValue ScriptEngine::classConstant(JSContext *ctx, JSValueConst this_val, int magic)
{
    const JSClassID id = JS_GetClassID(this_val);
    ScriptEngine *const that = ScriptEngine::scriptEngine();
    assert(that);
    auto it = that->mClasses.find(id);
    if (it == that->mClasses.end()) {
        return JS_ThrowTypeError(ctx, "Class can't be found");
    }

    const size_t offset = magic - ScriptEngine::ScriptClassData::FirstConstant;
    if (offset >= it->second->constants.size()) {
        return JS_ThrowTypeError(ctx, "Property can't be found");
    }

    ScriptValue ret = it->second->constants[offset].second.clone();
    if (ret) {
        if (ret.isError()) {
            return JS_Throw(that->mContext, ret.leakValue());
        }
        return ret.leakValue();
    }
    return *ScriptValue::undefined();
}

// static
JSValue ScriptEngine::classGetterSetterSetter(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic)
{
    const JSClassID id = JS_GetClassID(this_val);
    ScriptEngine *const that = ScriptEngine::scriptEngine();
    assert(that);
    auto it = that->mClasses.find(id);
    if (it == that->mClasses.end()) {
        return JS_ThrowTypeError(ctx, "Class can't be found");
    }

    const size_t offset = magic - ScriptEngine::ScriptClassData::FirstGetterSetter;
    if (offset >= it->second->getterSetters.size()) {
        return JS_ThrowTypeError(ctx, "Property can't be found");
    }

    ScriptClassInstance *instance = static_cast<ScriptClassInstance *>(JS_GetOpaque(this_val, id));
    ScriptValue value(JS_DupValue(ctx, val));
    std::optional<std::string> ret = it->second->getterSetters[offset].second.second(instance, std::move(value));
    if (ret) {
        return JS_ThrowTypeError(ctx, "Failed to set property %s on %s: %s",
                                 it->second->name.c_str(), it->second->name.c_str(), ret->c_str());
    }
    return *ScriptValue::undefined();
}

// static
JSValue ScriptEngine::classMethod(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic)
{
    const JSClassID id = JS_GetClassID(this_val);
    ScriptEngine *const that = ScriptEngine::scriptEngine();
    assert(that);
    auto it = that->mClasses.find(id);
    if (it == that->mClasses.end()) {
        return JS_ThrowTypeError(ctx, "Class can't be found");
    }

    if (static_cast<size_t>(magic) >= it->second->methods.size()) {
        return JS_ThrowTypeError(ctx, "Method can't be found");
    }

    std::vector<ScriptValue> args(argc);
    for (int i=0; i<argc; ++i) {
        args[i] = ScriptValue(JS_DupValue(ctx, argv[i]));
    }

    ScriptClassInstance *instance = static_cast<ScriptClassInstance *>(JS_GetOpaque(this_val, id));
    ScriptValue ret = it->second->methods[magic].second(instance, std::move(args));
    if (ret) {
        if (ret.isError()) {
            return JS_Throw(that->mContext, ret.leakValue());
        }
        return ret.leakValue();
    }

    return *ScriptValue::undefined();
}

ScriptValue ScriptEngine::setKeyEventHandler(std::vector<ScriptValue> &&args)
{
    if (args.empty() || !args[0].isFunction()) {
        return ScriptValue::makeError("Invalid arguments");
    }

    mKeyHandler = std::move(args[0]);
    return {};
}

ScriptValue ScriptEngine::setTimeoutImpl(EventLoop::TimerMode mode, std::vector<ScriptValue> &&args)
{
    if (args.empty() || !args[0].isFunction()) {
        return ScriptValue::makeError("First argument must be a function");
    }

    unsigned int ms = 0;
    if (args.size() > 1) {
        auto ret = args[1].toUint();
        if (ret.ok()) {
            ms = *ret;
        }
    }
    ScriptValue callback = std::move(args[0]);
    args.erase(args.begin(), args.begin() + 2);

    std::unique_ptr<TimerData> timerData = std::make_unique<TimerData>();
    timerData->timerMode = mode;
    timerData->args = std::move(args);
    timerData->callback = std::move(callback);
    const uint32_t id = mEventLoop->startTimer([this](uint32_t timerId) {
        const auto it = mTimers.find(timerId);
        if (it == mTimers.end()) {
            spdlog::error("Couldn't find timer with id {}", timerId);
            return;
        }
        auto data = std::move(it->second);
        mTimers.erase(it);
        data->callback.call(data->args);
    }, ms, mode);
    mTimers[id] = std::move(timerData);
    return ScriptValue(id);
}

ScriptValue ScriptEngine::setTimeout(std::vector<ScriptValue> &&args)
{
    return setTimeoutImpl(EventLoop::TimerMode::SingleShot, std::move(args));
}

ScriptValue ScriptEngine::setInterval(std::vector<ScriptValue> &&args)
{
    return setTimeoutImpl(EventLoop::TimerMode::Repeat, std::move(args));
}

ScriptValue ScriptEngine::clearTimeout(std::vector<ScriptValue> &&args)
{
    if (!args.empty()) {
        const auto id = args[0].toUint();
        if (id.ok()) {
            auto it = mTimers.find(*id);
            if (it != mTimers.end()) {
                mTimers.erase(it);
                mEventLoop->stopTimer(*id);
            }
        }
    }
    return ScriptValue();
}

ScriptValue ScriptEngine::exit(std::vector<ScriptValue> &&args)
{
    int exitCode = 0;
    if (args.size() > 0) {
        auto val = args[0].toInt();
        if (val.ok()) {
            exitCode = *val;
        }
    }

    mOnExit.emit(exitCode);
    return {};
}

ScriptEngine::ProcessData::~ProcessData()
{
    if (proc) {
        uv_close(reinterpret_cast<uv_handle_t *>(&*proc), nullptr);
    }

    if (stdinPipe) {
        uv_close(reinterpret_cast<uv_handle_t*>(&*stdinPipe), nullptr);
    }

    if (stdoutPipe) {
        uv_close(reinterpret_cast<uv_handle_t*>(&*stdoutPipe), nullptr);
    }

    if (stderrPipe) {
        uv_close(reinterpret_cast<uv_handle_t*>(&*stderrPipe), nullptr);
    }

}
} // namespace spurv
