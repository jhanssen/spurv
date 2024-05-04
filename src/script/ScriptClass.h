#pragma once

#include <functional>
#include <string>
#include <variant>
#include <vector>
#include <ScriptValue.h>
#include <UnorderedDense.h>

namespace spurv {
class ScriptClassInstance
{
public:
    virtual ~ScriptClassInstance();
};

class ScriptClass
{
public:
    // ### std::optional<std::string> should be some error class
    using Constructor = std::function<std::optional<std::string>(ScriptClassInstance *&instance, std::vector<ScriptValue> &&args)>;
    using Method = std::function<ScriptValue(ScriptClassInstance *instance, std::vector<ScriptValue> &&args)>;
    using Getter = std::function<ScriptValue(ScriptClassInstance *instance)>;
    using Setter = std::function<std::optional<std::string>(ScriptClassInstance *instance, ScriptValue &&value)>;
    using StaticMethod = std::function<ScriptValue(std::vector<ScriptValue> &&args)>;

    ScriptClass(const std::string &name, Constructor &&constructor);
    ScriptClass &operator=(ScriptClass &&other) = delete;

    const std::string &name() const;
    const Constructor &constructor() const;
    Constructor &constructor();
    const unordered_dense::map<std::string, Method> &methods() const;
    unordered_dense::map<std::string, Method> &methods();
    const unordered_dense::map<std::string, std::variant<ScriptValue, Getter, std::pair<Getter, Setter>>> &properties() const;
    unordered_dense::map<std::string, std::variant<ScriptValue, Getter, std::pair<Getter, Setter>>> &properties();

    const unordered_dense::map<std::string, StaticMethod> &staticMethods() const;
    unordered_dense::map<std::string, StaticMethod> &staticMethods();
    const unordered_dense::map<std::string, ScriptValue> &staticProperties() const;
    unordered_dense::map<std::string, ScriptValue> &staticProperties();

    void addMethod(const std::string &name, Method &&method);
    void addProperty(const std::string &name, ScriptValue &&value);
    void addProperty(const std::string &name, Getter &&getter);
    void addProperty(const std::string &name, Getter &&getter, Setter &&setter);
    void addStaticMethod(const std::string &name, StaticMethod &&method);
    void addStaticProperty(const std::string &name, ScriptValue &&value);
private:
    std::string mName;
    Constructor mConstructor;
    unordered_dense::map<std::string, Method> mMethods;
    unordered_dense::map<std::string, std::variant<ScriptValue, Getter, std::pair<Getter, Setter>>> mProperties;
    unordered_dense::map<std::string, StaticMethod> mStaticMethods;
    unordered_dense::map<std::string, ScriptValue> mStaticProperties;
};
} // namespace spurv
