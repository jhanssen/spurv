#include "ScriptEngine.h"
#include "Builtins.h"
#include <Keys.h>
#include <Editor.h>

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

    Editor *editor = Editor::instance();
    {
        const int argc = editor->argc();
        char **const argv = editor->argv();
        std::vector<ScriptValue> args(argc);
        for (int i=0; i<argc; ++i) {
            args[i] = ScriptValue(argv[i]);
        }
        mSpurv.setProperty(mAtoms.argv, ScriptValue(std::move(args)));
    }

    {
        std::vector<std::pair<std::string, ScriptValue>> object;
        char **envp = editor->envp();
        while (*envp) {
            char *eq = strchr(*envp, '=');
            if (eq) {
                object.push_back(std::make_pair(std::string(*envp, eq), ScriptValue(eq + 1)));
            } else {
                object.push_back(std::make_pair(std::string(*envp), ScriptValue(std::string())));
            }
            ++envp;
        }
        mSpurv.setProperty(mAtoms.env, ScriptValue(std::move(object)));
    }

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
    bindGlobalFunction(mAtoms.queueMicrotask, std::bind(&ScriptEngine::queueMicrotask, this, std::placeholders::_1));
    bindGlobalFunction(mAtoms.setInterval, std::bind(&ScriptEngine::setInterval, this, std::placeholders::_1));
    bindGlobalFunction(mAtoms.clearTimeout, std::bind(&ScriptEngine::clearTimeout, this, std::placeholders::_1));
    bindGlobalFunction(mAtoms.clearInterval, std::bind(&ScriptEngine::clearTimeout, this, std::placeholders::_1));
    bindGlobalFunction(mAtoms.atob, &Builtins::atob);
    bindGlobalFunction(mAtoms.btoa, &Builtins::btoa);
}

ScriptEngine::~ScriptEngine()
{
    mTimers.clear();
    mFunctions.clear();
    mSpurv.clear();
    mGlobal.clear();
    mProcessHandler.clear();
    mKeyHandler.clear();
    mClasses.clear();

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

void ScriptEngine::start()
{
    const std::filesystem::path file = mAppPath / "../src/typescript/dist/spurv.js";
    auto ret = eval(file);
    if (ret.hasError()) {
        spdlog::critical("{}", ret.error().message);
    }
}

void ScriptEngine::onKey(int key, int scancode, int action, int mods, const std::optional<std::string> &keyName)
{
    assert(EventLoop::eventLoop() == mEventLoop);
    if (!mKeyHandler.isFunction()) {
        spdlog::error("Got key but no key handler key: {} scancode: {} action: {} mods: {} name: {}\n",
                      key, scancode, action, mods, keyName.value_or("unknown"));
        return;
    }
    ScriptValue object(ScriptValue::Object);
    object.setProperty(mAtoms.key, ScriptValue(key));
    object.setProperty(mAtoms.scancode, ScriptValue(scancode));
    object.setProperty(mAtoms.action, ScriptValue(action));
    object.setProperty(mAtoms.mods, ScriptValue(mods));
    if (keyName) {
        object.setProperty(mAtoms.keyName, ScriptValue(*keyName));
    }
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
    CallScope scope(this);
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
    const int magic = mFunctions.size();
    std::unique_ptr<FunctionData> data = std::make_unique<FunctionData>();
    data->value = ScriptValue(JS_NewCFunctionData(mContext, bindHelper, 0, magic, 0, nullptr));
    data->function = std::move(function);
    ScriptValue ret = data->value.clone();
    mFunctions.push_back(std::move(data));
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

    JSClassID id = 0;
    JS_NewClassID(&id);
    if (JS_NewClass(mRuntime, id, &def) != 0) {
        return false;
    }

    // const auto &ctor = data->clazz.constructor();
    auto &properties = clazz.properties();
    auto &methods = clazz.methods();
    data->protoProperties.resize(properties.size() + methods.size());
    memset(data->protoProperties.data(), 0, data->protoProperties.size() * sizeof(JSCFunctionListEntry));
    size_t constantCount = 0;
    size_t getterCount = 0;
    size_t getterSetterCount = 0;
    for (auto &prop : properties) {
        if (std::holds_alternative<ScriptValue>(prop.second)) {
            ++constantCount;
        } else if (std::holds_alternative<ScriptClass::Getter>(prop.second)) {
            ++getterCount;
        } else {
            ++getterSetterCount;
        }
    }
    data->constants.reserve(constantCount);
    data->getters.reserve(getterCount);
    data->getterSetters.reserve(getterSetterCount);
    // make sure we get no reallocs to avoid the c_str's being moved, could
    // really strdup instead
    size_t idx = 0;

    for (auto &prop : properties) {
        auto &protoProp = data->protoProperties[idx++];
        protoProp.prop_flags = JS_PROP_C_W_E;
        protoProp.def_type = JS_DEF_CGETSET_MAGIC;
        if (std::holds_alternative<ScriptValue>(prop.second)) {
            protoProp.prop_flags &= ~JS_PROP_WRITABLE;
            protoProp.u.getset.get.getter_magic = &ScriptEngine::classConstant;
            protoProp.magic = static_cast<int16_t>(data->constants.size());
            data->constants.push_back({ std::move(prop.first), std::get<ScriptValue>(std::move(prop.second)) });
            protoProp.name = data->constants.back().name.c_str();
        } else if (std::holds_alternative<ScriptClass::Getter>(prop.second)) {
            protoProp.prop_flags &= ~JS_PROP_WRITABLE;
            protoProp.u.getset.get.getter_magic = &ScriptEngine::classGetter;
            protoProp.magic = static_cast<int16_t>(data->getters.size());
            data->getters.push_back({ std::move(prop.first), std::get<ScriptClass::Getter>(std::move(prop.second)) });
            protoProp.name = data->getters.back().name.c_str();
        } else {
            protoProp.u.getset.get.getter_magic = &ScriptEngine::classGetter;
            protoProp.u.getset.set.setter_magic = &ScriptEngine::classGetterSetterSetter;
            protoProp.magic = static_cast<int16_t>(data->getterSetters.size());
            std::pair<ScriptClass::Getter, ScriptClass::Setter> getterSetter = std::get<std::pair<ScriptClass::Getter, ScriptClass::Setter>>(std::move(prop.second));
            data->getterSetters.push_back({ std::move(prop.first), std::move(getterSetter.first), std::move(getterSetter.second) });
            protoProp.name = data->getterSetters.back().name.c_str();
        }
    }
    data->construct = std::move(clazz.constructor());

    data->methods.reserve(methods.size());
    for (auto &method : methods) {
        auto &protoProp = data->protoProperties[idx++];
        protoProp.prop_flags = JS_PROP_C_W_E;
        protoProp.def_type = JS_DEF_CFUNC;
        protoProp.magic = static_cast<int16_t>(data->methods.size());
        auto &func = protoProp.u.func;
        func.length = 1;
        func.cproto = JS_CFUNC_generic_magic;
        func.cfunc.generic_magic = &ScriptEngine::classMethod;
        data->methods.push_back({ std::move(method.first), std::move(method.second) });
        protoProp.name = data->methods.back().name.c_str();
    }
    assert(idx == data->protoProperties.size());

    data->prototype = ScriptValue(JS_NewObject(mContext));
    JS_SetPropertyFunctionList(mContext, *data->prototype, data->protoProperties.data(), data->protoProperties.size());

    const int magic = mConstructors.size();
    data->constructor = ScriptValue(JS_NewCFunctionData(mContext, constructHelper, 0, magic, 0, nullptr));

    auto &staticProperties = clazz.staticProperties();
    auto &staticMethods = clazz.staticMethods();
    data->staticProperties.resize(staticProperties.size() + staticMethods.size());
    memset(data->staticProperties.data(), 0, data->staticProperties.size() * sizeof(JSCFunctionListEntry));

    idx = 0;
    for (auto &prop : staticProperties) {
        auto &staticProp = data->staticProperties[idx++];
        staticProp.prop_flags = JS_PROP_C_W_E;
        staticProp.def_type = JS_DEF_CGETSET_MAGIC;
        staticProp.prop_flags &= ~JS_PROP_WRITABLE;
        staticProp.u.getset.get.getter_magic = &ScriptEngine::classStaticConstant;
        staticProp.magic = static_cast<int16_t>(mStaticClassConstants.size());
        auto constant = std::make_unique<ScriptClassData::Constant>();
        constant->name = std::move(prop.first);
        constant->value = std::move(prop.second);
        staticProp.name = constant->name.c_str();
        mStaticClassConstants.push_back(std::move(constant));
    }

    for (auto &method : staticMethods) {
        auto &staticProp = data->staticProperties[idx++];
        staticProp.prop_flags = JS_PROP_C_W_E;
        staticProp.def_type = JS_DEF_CFUNC;
        // ### is 2^15 enough static functions?
        staticProp.magic = static_cast<int16_t>(mStaticClassMethods.size());
        auto &func = staticProp.u.func;
        func.length = 1;
        func.cproto = JS_CFUNC_generic_magic;
        func.cfunc.generic_magic = &ScriptEngine::classStaticMethod;
        auto staticMethod = std::make_unique<StaticMethod>();
        staticMethod->name = std::move(method.first);
        staticMethod->call = std::move(method.second);
        staticProp.name = staticMethod->name.c_str();
        mStaticClassMethods.push_back(std::move(staticMethod));
    }

    JS_SetConstructorBit(mContext, *data->constructor, 1);
    JS_SetConstructor(mContext, *data->constructor, *data->prototype);
    JS_SetClassProto(mContext, id, JS_DupValue(mContext, *data->prototype));

    mConstructors.push_back(id);

    // spdlog::error(data->name);
    mSpurv.setProperty(data->name, data->constructor.clone());
    mClasses[id] = std::move(data);

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
        { "Int32Array", ScriptBufferSource::Int32Array },
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

// static
JSValue ScriptEngine::bindHelper(JSContext *ctx, JSValueConst /*funcObject*/, int argc, JSValueConst *argv, int magic, JSValue *)
{
    ScriptEngine *that = ScriptEngine::scriptEngine();
    if (static_cast<size_t>(magic) >= that->mFunctions.size()) {
        return JS_ThrowTypeError(ctx, "Function can't be found");
    }

    std::vector<ScriptValue> args(argc);
    for (int i=0; i<argc; ++i) {
        args[i] = ScriptValue(JS_DupValue(that->mContext, argv[i]));
    }

    const auto &func = that->mFunctions[magic];
    ScriptValue ret = func->function(std::move(args));
    if (ret) {
        if (ret.isError()) {
            return JS_Throw(that->mContext, ret.leakValue());
        }
        return ret.leakValue();
    }
    return *ScriptValue::undefined();
}

// static
JSValue ScriptEngine::constructHelper(JSContext *ctx, JSValueConst /*funcObject*/, int argc, JSValueConst *argv, int magic, JSValue *)
{
    ScriptEngine *that = ScriptEngine::scriptEngine();
    if (static_cast<size_t>(magic) >= that->mConstructors.size()) {
        return JS_ThrowTypeError(ctx, "Constructor can't be found (1)");
    }

    const JSClassID classId = that->mConstructors[magic];

    auto it2 = that->mClasses.find(classId);
    if (it2 == that->mClasses.end()) {
        spdlog::error("ClassId is {}, known are {}", classId, keys(that->mClasses));
        return JS_ThrowTypeError(ctx, "Constructor can't be found (2)");
    }

    std::vector<ScriptValue> args(argc);
    for (int i=0; i<argc; ++i) {
        args[i] = ScriptValue(JS_DupValue(that->mContext, argv[i]));
    }

    ScriptClassInstance *instance = nullptr;
    std::optional<std::string> ret = it2->second->construct(instance, std::move(args));
    if (ret) {
        delete instance;
        return JS_ThrowTypeError(ctx, "Failed to construct class %s: %s", it2->second->name.c_str(), ret->c_str());
    }
    JSValue val = JS_NewObjectClass(ctx, classId);
    JS_SetOpaque(val, instance);
    return val;
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
        if (id == 1 /* JS_CLASS_OBJECT */ && JS_HasProperty(ctx, this_val, that->mAtoms.constructor)) {
            // probably one of our prototypes that was stringified or something
            return *ScriptValue::undefined();
        }
        return JS_ThrowTypeError(ctx, "Class can't be found %s", fmt::format("{} vs {}", id, keys(that->mClasses)).c_str());
    }

    if (static_cast<size_t>(magic) >= it->second->getters.size()) {
        return JS_ThrowTypeError(ctx, "Property can't be found");
    }

    ScriptClassInstance *instance = static_cast<ScriptClassInstance *>(JS_GetOpaque(this_val, id));
    ScriptValue ret = it->second->getters[magic].get(instance);
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
        if (id == 1 /* JS_CLASS_OBJECT */ && JS_HasProperty(ctx, this_val, that->mAtoms.constructor)) {
            // probably one of our prototypes that was stringified or something
            return *ScriptValue::undefined();
        }
        return JS_ThrowTypeError(ctx, "Class can't be found %s", fmt::format("{} vs {}", id, keys(that->mClasses)).c_str());
    }

    if (static_cast<size_t>(magic) >= it->second->getterSetters.size()) {
        return JS_ThrowTypeError(ctx, "Property can't be found");
    }

    ScriptClassInstance *instance = static_cast<ScriptClassInstance *>(JS_GetOpaque(this_val, id));
    ScriptValue ret = it->second->getterSetters[magic].get(instance);
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
        if (id == 1 /* JS_CLASS_OBJECT */ && JS_HasProperty(ctx, this_val, that->mAtoms.constructor)) {
            // probably one of our prototypes that was stringified or something
            return *ScriptValue::undefined();
        }
        return JS_ThrowTypeError(ctx, "Class can't be found %s", fmt::format("{} vs {}", id, keys(that->mClasses)).c_str());
    }

    if (static_cast<size_t>(magic) >= it->second->constants.size()) {
        return JS_ThrowTypeError(ctx, "Property can't be found");
    }

    ScriptValue ret = it->second->constants[magic].value.clone();
    if (ret) {
        if (ret.isError()) {
            return JS_Throw(that->mContext, ret.leakValue());
        }
        return ret.leakValue();
    }
    return *ScriptValue::undefined();
}

// static
JSValue ScriptEngine::classStaticConstant(JSContext *ctx, JSValueConst /*this_val*/, int magic)
{
    ScriptEngine *const engine = ScriptEngine::scriptEngine();
    if (static_cast<size_t>(magic) >= engine->mStaticClassConstants.size()) {
        return JS_ThrowTypeError(ctx, "Static property can't be found");
    }
    ScriptValue ret = engine->mStaticClassConstants[magic]->value.clone();
    if (ret) {
        if (ret.isError()) {
            return JS_Throw(ctx, ret.leakValue());
        }
        return ret.leakValue();
    }
    return *ScriptValue::undefined();
}

// static
JSValue ScriptEngine::classStaticMethod(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv, int magic)
{
    ScriptEngine *const engine = ScriptEngine::scriptEngine();
    if (static_cast<size_t>(magic) >= engine->mStaticClassMethods.size()) {
        return JS_ThrowTypeError(ctx, "Static method can't be found");
    }

    std::vector<ScriptValue> args(argc);
    for (int i=0; i<argc; ++i) {
        args[i] = ScriptValue(JS_DupValue(ctx, argv[i]));
    }

    ScriptValue ret = engine->mStaticClassMethods[magic]->call(std::move(args));
    if (ret) {
        if (ret.isError()) {
            return JS_Throw(ctx, ret.leakValue());
        }
        return ret.leakValue();
    }

    return *ScriptValue::undefined();
}

// static
JSValue ScriptEngine::queuedMicrotask(JSContext *ctx, int argc, JSValueConst *argv)
{
    (void)argc;
    assert(argc == 1);
    assert(JS_IsFunction(ctx, argv[0]));
    ScriptEngine::CallScope scope(ScriptEngine::scriptEngine()); // ### should this be here?
    JSValue ret = JS_Call(scope.context, argv[0], *ScriptValue::undefined(), 0, nullptr);
    JS_FreeValue(ctx, argv[0]);
    return ret;
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


    if (static_cast<size_t>(magic) >= it->second->getterSetters.size()) {
        return JS_ThrowTypeError(ctx, "Property can't be found");
    }

    ScriptClassInstance *instance = static_cast<ScriptClassInstance *>(JS_GetOpaque(this_val, id));
    ScriptValue value(JS_DupValue(ctx, val));
    std::optional<std::string> ret = it->second->getterSetters[magic].set(instance, std::move(value));
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
    ScriptValue ret = it->second->methods[magic].call(instance, std::move(args));
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

ScriptValue ScriptEngine::queueMicrotask(std::vector<ScriptValue> &&args)
{
    if (args.empty() || !args[0].isFunction()) {
        return ScriptValue::makeError("First argument must be a function");
    }

    JSValue value = args[0].leakValue();
    JS_EnqueueJob(mContext, &ScriptEngine::queuedMicrotask, 1, &value);
    return {};
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

void ScriptEngine::executeMicrotasks()
{
    while (true) {
        JSContext *ctx = nullptr;
        int ret =  JS_ExecutePendingJob(mRuntime, &ctx);
        if (!ret) {
            break;
        }
        if (ret < 0) {
            ScriptValue exception(JS_GetException(mContext));
            // ### need a nice print exception
            ScriptValue stack = exception.getProperty("stack");
            spdlog::error("Got exception in microtask {}\n{}",
                          exception.toString(), stack.toString());
        }
    }
}
} // namespace spurv
