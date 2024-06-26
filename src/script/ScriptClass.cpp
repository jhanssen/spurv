#include "ScriptClass.h"

namespace spurv {

ScriptClass::ScriptClass(const std::string &name, Constructor &&constructor)
    : mName(name), mConstructor(std::move(constructor))
{
}

void ScriptClass::addMethod(const std::string &name, Method &&method)
{
    mMethods[name] = std::move(method);
}

void ScriptClass::addProperty(const std::string &name, ScriptValue &&value)
{
    mProperties[name] = std::move(value);
}

void ScriptClass::addProperty(const std::string &name, Getter &&getter)
{
    mProperties[name] = std::move(getter);
}

void ScriptClass::addProperty(const std::string &name, Getter &&getter, Setter &&setter)
{
    mProperties[name] = std::make_pair(std::move(getter), std::move(setter));
}

void ScriptClass::addStaticMethod(const std::string &name, StaticMethod &&method)
{
    mStaticMethods[name] = std::move(method);
}

void ScriptClass::addStaticProperty(const std::string &name, ScriptValue &&value)
{
    mStaticProperties[name] = std::move(value);
}

const std::string &ScriptClass::name() const
{
    return mName;
}

const ScriptClass::Constructor &ScriptClass::constructor() const
{
    return mConstructor;
}

ScriptClass::Constructor &ScriptClass::constructor()
{
    return mConstructor;
}

const unordered_dense::map<std::string, ScriptClass::Method> &ScriptClass::methods() const
{
    return mMethods;
}

unordered_dense::map<std::string, ScriptClass::Method> &ScriptClass::methods()
{
    return mMethods;
}

const unordered_dense::map<std::string, std::variant<ScriptValue, ScriptClass::Getter, std::pair<ScriptClass::Getter, ScriptClass::Setter>>> &ScriptClass::properties() const
{
    return mProperties;
}

unordered_dense::map<std::string, std::variant<ScriptValue, ScriptClass::Getter, std::pair<ScriptClass::Getter, ScriptClass::Setter>>> &ScriptClass::properties()
{
    return mProperties;
}

const unordered_dense::map<std::string, ScriptClass::StaticMethod> &ScriptClass::staticMethods() const
{
    return mStaticMethods;
}

unordered_dense::map<std::string, ScriptClass::StaticMethod> &ScriptClass::staticMethods()
{
    return mStaticMethods;
}

const unordered_dense::map<std::string, ScriptValue> &ScriptClass::staticProperties() const
{
    return mStaticProperties;
}

unordered_dense::map<std::string, ScriptValue> &ScriptClass::staticProperties()
{
    return mStaticProperties;
}
} // namespace spurv
