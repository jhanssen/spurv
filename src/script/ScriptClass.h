#pragma

#include <ScriptValue.h>

class ScriptClass
{
public:
    using Getter = std::function<ScriptValue()>;
    using Setter = std::function<ScriptValue(ScriptValue &&value)>;

    ScriptClass(const std::string &name);

    void addFunction(const std::string &name, ScriptValue::Function &&function);
    void addProperty(const std::string &name, ScriptValue &&value);
    void addProperty(const std::string &name, Getter &&getter);
    void addProperty(const std::string &name, Getter &&getter, Setter &&setter);
};
