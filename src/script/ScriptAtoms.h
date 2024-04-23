#pragma once

#include <quickjs.h>

struct ScriptAtoms {
#define ScriptAtom(atom)                       \
    JSAtom atom;
#include "ScriptAtomsInternal.h"
    FOREACH_SCRIPTATOM(ScriptAtom)
#undef ScriptAtom
};
