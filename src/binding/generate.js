#!/usr/bin/env node
"use strict";

// Binding generator: parses .d.ts files with the TypeScript compiler API and
// emits N-API C++ bindings for the symbols annotated with @native.
//
// usage: node generate.js --dts <file.d.ts...> --out <dir>
//
// The .d.ts files are declared in src/binding/CMakeLists.txt (SPURV_BINDING_DTS).

const ts = require("typescript");
const fs = require("fs");
const path = require("path");

// ---------------------------------------------------------------------------
// command line

function parseArgs(argv) {
    const args = { dts: [], out: null };
    let i = 0;
    while (i < argv.length) {
        const a = argv[i];
        if (a === "--dts") {
            ++i;
            while (i < argv.length && !argv[i].startsWith("--")) {
                args.dts.push(argv[i]);
                ++i;
            }
        } else if (a === "--out") {
            args.out = argv[++i];
            ++i;
        } else {
            console.error("generate.js: unknown argument: " + a);
            process.exit(1);
        }
    }
    if (args.dts.length === 0 || args.out === null) {
        console.error("usage: node generate.js --dts <file.d.ts...> --out <dir>");
        process.exit(1);
    }
    return args;
}

// ---------------------------------------------------------------------------
// jsdoc annotations

function tagText(tag) {
    if (tag.comment === undefined) {
        return "";
    }
    if (typeof tag.comment === "string") {
        return tag.comment.trim();
    }
    return tag.comment
        .filter((c) => c.kind === ts.SyntaxKind.JSDocText)
        .map((c) => c.text)
        .join(" ")
        .trim();
}

function getTag(node, name) {
    return ts.getJSDocTags(node).find((t) => t.tagName.text === name);
}

// Returns the @native value, or undefined when the symbol is not annotated as
// native (in which case no binding is generated for it).
function nativeName(node, fallback) {
    const tag = getTag(node, "native");
    if (tag === undefined) {
        return undefined;
    }
    const value = tagText(tag);
    return value.length > 0 ? value : fallback;
}

function headerName(node) {
    const tag = getTag(node, "header");
    return tag === undefined ? undefined : tagText(tag);
}

// ---------------------------------------------------------------------------
// type mapping (ts type node -> cpp type model)

class Unsupported extends Error {}
class AsyncReturn extends Error {}

function isUndefinedOrNullType(t) {
    if (t.kind === ts.SyntaxKind.UndefinedKeyword) {
        return true;
    }
    return (
        t.kind === ts.SyntaxKind.LiteralTypeNode &&
        (t.literal.kind === ts.SyntaxKind.NullKeyword || t.literal.kind === ts.SyntaxKind.UndefinedKeyword)
    );
}

// TypeScript 6.0 renamed TypeReferenceNode -> TypeReference; be tolerant of both.
const TypeRefKind = ts.SyntaxKind.TypeReference !== undefined ? ts.SyntaxKind.TypeReference : ts.SyntaxKind.TypeReferenceNode;

function mapTypeNode(tn, checker) {
    const k = tn.kind;
    if (k === ts.SyntaxKind.StringKeyword) return { tag: "string" };
    if (k === ts.SyntaxKind.NumberKeyword) return { tag: "number" };
    if (k === ts.SyntaxKind.BooleanKeyword) return { tag: "boolean" };
    if (k === ts.SyntaxKind.VoidKeyword) return { tag: "void" };
    if (k === TypeRefKind) {
        if (tn.typeName.kind !== ts.SyntaxKind.Identifier) {
            throw new Unsupported("qualified name " + tn.typeName.getText());
        }
        const name = tn.typeName.text;
        if (name === "Promise") {
            throw new AsyncReturn();
        }
        const symbol = checker.getSymbolAtLocation(tn.typeName);
        if (symbol !== undefined && symbol.declarations !== undefined && symbol.declarations.length > 0) {
            const decl = symbol.declarations[0];
            if (decl.kind === ts.SyntaxKind.EnumDeclaration) {
                const native = nativeName(decl, undefined);
                return { tag: "enum", cpp: native !== undefined ? native : "int32_t", tsName: name };
            }
            if (decl.kind === ts.SyntaxKind.ClassDeclaration) {
                const native = nativeName(decl, "spurv::" + name);
                if (native === undefined) {
                    throw new Unsupported("unbound class reference " + name);
                }
                return { tag: "class", cpp: native, tsName: name };
            }
        }
        throw new Unsupported("type reference " + name);
    }
    if (k === ts.SyntaxKind.UnionTypeNode) {
        const members = tn.types.filter((t) => !isUndefinedOrNullType(t));
        if (members.length !== 1) {
            throw new Unsupported("union type");
        }
        return Object.assign({}, mapTypeNode(members[0], checker), { optional: true });
    }
    throw new Unsupported("type node " + ts.SyntaxKind[k]);
}

// ---------------------------------------------------------------------------
// model collection

function collectEnum(node, checker, model) {
    const isConst = node.modifiers !== undefined && node.modifiers.some((m) => m.kind === ts.SyntaxKind.ConstKeyword);
    if (!isConst) {
        model.warned.push("skip enum " + node.name.text + ": only const enums are supported");
        return;
    }
    const members = [];
    for (const member of node.members) {
        const value = checker.getConstantValue(member);
        if (value === undefined) {
            model.warned.push("skip enum " + node.name.text + "." + member.name.getText() + ": non-constant value");
            continue;
        }
        members.push({ name: member.name.getText(), value: value });
    }
    model.enums.push({ name: node.name.text, members: members });
}

function collectClass(node, checker, model) {
    const cpp = nativeName(node, "spurv::" + node.name.text);
    if (cpp === undefined) {
        return; // not annotated -> not bound
    }
    const cls = {
        tsName: node.name.text,
        cppName: cpp,
        header: headerName(node),
        hasCtor: false,
        members: [],
    };
    for (const member of node.members) {
        if (member.kind === ts.SyntaxKind.Constructor) {
            cls.hasCtor = true;
            continue;
        }
        if (
            member.kind !== ts.SyntaxKind.PropertyDeclaration &&
            member.kind !== ts.SyntaxKind.MethodDeclaration
        ) {
            continue; // static members / accessors: not handled yet
        }
        const native = nativeName(member, undefined);
        if (native === undefined) {
            continue; // member not annotated -> not bound
        }
        try {
            if (member.kind === ts.SyntaxKind.PropertyDeclaration) {
                const type = mapTypeNode(member.type, checker);
                const readonly =
                    member.modifiers !== undefined &&
                    member.modifiers.some((m) => m.kind === ts.SyntaxKind.ReadonlyKeyword);
                const getter = native.length > 0 ? native : member.name.getText();
                const setter = readonly
                    ? undefined
                    : "set" + getter[0].toUpperCase() + getter.slice(1);
                cls.members.push({ kind: "property", tsName: member.name.getText(), getter: getter, setter: setter, type: type });
            } else {
                const params = member.parameters.map((p) => {
                    const type = mapTypeNode(p.type, checker);
                    return { name: p.name.getText(), type: type };
                });
                const ret = mapTypeNode(member.type, checker);
                const name = native.length > 0 ? native : member.name.getText();
                cls.members.push({ kind: "method", tsName: member.name.getText(), cppName: name, params: params, ret: ret });
            }
        } catch (e) {
            if (e instanceof Unsupported || e instanceof AsyncReturn) {
                model.warned.push("skip " + cls.tsName + "." + member.name.getText() + ": " + e.message);
                continue;
            }
            throw e;
        }
    }
    model.classes.push(cls);
}

function collectFunction(node, checker, model) {
    const cpp = nativeName(node, "spurv::" + node.name.text);
    if (cpp === undefined) {
        return;
    }
    try {
        const params = node.parameters.map((p) => {
            const type = mapTypeNode(p.type, checker);
            return { name: p.name.getText(), type: type };
        });
        const ret = mapTypeNode(node.type, checker);
        model.functions.push({ tsName: node.name.text, cppName: cpp, params: params, ret: ret });
    } catch (e) {
        if (e instanceof Unsupported || e instanceof AsyncReturn) {
            model.warned.push("skip function " + node.name.getText() + ": " + e.message);
            return;
        }
        throw e;
    }
}

function collect(model, checker, sourceFile) {
    for (const stmt of sourceFile.statements) {
        if (stmt.kind === ts.SyntaxKind.ModuleDeclaration && stmt.name.text === "spurv") {
            const body = stmt.body;
            if (body === undefined || body.kind !== ts.SyntaxKind.ModuleBlock) {
                continue;
            }
            for (const decl of body.statements) {
                switch (decl.kind) {
                    case ts.SyntaxKind.EnumDeclaration:
                        collectEnum(decl, checker, model);
                        break;
                    case ts.SyntaxKind.ClassDeclaration:
                        collectClass(decl, checker, model);
                        break;
                    case ts.SyntaxKind.FunctionDeclaration:
                        collectFunction(decl, checker, model);
                        break;
                    default:
                        break; // interfaces, consts, type aliases etc: not handled yet
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// code emission

const INDENT = "    ";

function emitReturn(type, expr) {
    switch (type.tag) {
        case "void":
            return ["return spurv::binding::undefined(env);"];
        case "string":
            return ["return spurv::binding::toJsString(env, " + expr + ");"];
        case "number":
            return ["return spurv::binding::toJsNumber(env, static_cast<double>(" + expr + "));"];
        case "boolean":
            return ["return spurv::binding::toJsBoolean(env, " + expr + ");"];
        case "enum":
            return [
                "return spurv::binding::toJsNumber(env, static_cast<double>(static_cast<std::int32_t>(" + expr + ")));",
            ];
        case "class":
            return [
                "std::shared_ptr<" + type.cpp + "> value = " + expr + ";",
                "if (!value) {",
                "    return spurv::binding::undefined(env);",
                "}",
                "return " + type.tsName + "_wrapBorrowed(env, value.get());",
            ];
    }
    return ["return spurv::binding::undefined(env);"];
}

function emitParam(type, name, optional, index) {
    const access = optional
        ? "argc > " + index + " ? args[" + index + "] : spurv::binding::undefined(env)"
        : "args[" + index + "]";
    switch (type.tag) {
        case "string":
            return { lines: ["std::string " + name + " = spurv::binding::fromJsString(env, " + access + ");"], expr: name };
        case "number":
            return { lines: ["double " + name + " = spurv::binding::fromJsNumber(env, " + access + ");"], expr: name };
        case "boolean":
            return { lines: ["bool " + name + " = spurv::binding::fromJsBoolean(env, " + access + ");"], expr: name };
        case "enum":
            return {
                lines: [type.cpp + " " + name + " = static_cast<" + type.cpp + ">(spurv::binding::fromJsInt32(env, " + access + "));"],
                expr: name,
            };
        case "class":
            return {
                lines: [
                    type.cpp + "* " + name + " = spurv::binding::unwrapMaybe<" + type.cpp + ">(env, " + access + ");",
                    "std::shared_ptr<" + type.cpp + "> " + name + "Shared;",
                    "if (" + name + " != nullptr) {",
                    "    " + name + "Shared = std::shared_ptr<" + type.cpp + ">(" + name + ", [](" + type.cpp + "*) {});",
                    "}",
                ],
                expr: name + "Shared",
            };
    }
    return { lines: [], expr: "{}" };
}

function paramLinesAndExprs(params, outLines) {
    const exprs = [];
    params.forEach((p, i) => {
        const res = emitParam(p.type, p.name, p.optional === true, i);
        res.lines.forEach((l) => outLines.push(INDENT + l));
        exprs.push(res.expr);
    });
    return exprs;
}

function emitEnumInit(en) {
    const lines = [];
    lines.push("static napi_value Init_" + en.name + "(napi_env env)");
    lines.push("{");
    lines.push(INDENT + "napi_value obj = nullptr;");
    lines.push(INDENT + "SPURV_NAPI_CALL(napi_create_object(env, &obj));");
    for (const m of en.members) {
        lines.push(INDENT + "{");
        lines.push(INDENT + "    napi_value v = nullptr;");
        if (typeof m.value === "number") {
            lines.push(INDENT + "    SPURV_NAPI_CALL(napi_create_int32(env, " + m.value + ", &v));");
        } else {
            lines.push(INDENT + '    SPURV_NAPI_CALL(napi_create_string_utf8(env, "' + m.value + '", NAPI_AUTO_LENGTH, &v));');
        }
        lines.push(INDENT + '    SPURV_NAPI_CALL(napi_set_named_property(env, obj, "' + m.name + '", v));');
        lines.push(INDENT + "}");
    }
    lines.push(INDENT + "return obj;");
    lines.push("}");
    return lines;
}

function emitClassCtor(cls) {
    const lines = [];
    lines.push("static napi_value " + cls.tsName + "_constructor(napi_env env, napi_callback_info info)");
    lines.push("{");
    lines.push(INDENT + "napi_value jsThis = nullptr;");
    lines.push(INDENT + "SPURV_NAPI_CALL(napi_get_cb_info(env, info, nullptr, nullptr, &jsThis, nullptr));");
    lines.push("");
    lines.push(INDENT + cls.cppName + "* ptr = static_cast<" + cls.cppName + "*>(spurv::binding::takeBorrowStash());");
    lines.push(INDENT + "const bool owned = ptr == nullptr;");
    lines.push(INDENT + "if (ptr == nullptr) {");
    lines.push(INDENT + "    ptr = new " + cls.cppName + "();");
    lines.push(INDENT + "}");
    lines.push(INDENT + "spurv::binding::wrap(env, jsThis, ptr, owned);");
    lines.push(INDENT + "return jsThis;");
    lines.push("}");
    return lines;
}

function emitWrapBorrowed(cls) {
    const lines = [];
    lines.push("static napi_value " + cls.tsName + "_wrapBorrowed(napi_env env, " + cls.cppName + "* ptr)");
    lines.push("{");
    lines.push(INDENT + "spurv::binding::setBorrowStash(ptr);");
    lines.push(INDENT + "napi_value ctor = nullptr;");
    lines.push(INDENT + "SPURV_NAPI_CALL(napi_get_reference_value(env, g_" + cls.tsName + "_ctor, &ctor));");
    lines.push(INDENT + "napi_value instance = nullptr;");
    lines.push(INDENT + "SPURV_NAPI_CALL(napi_new_instance(env, ctor, 0, nullptr, &instance));");
    lines.push(INDENT + "return instance;");
    lines.push("}");
    return lines;
}

function emitGetter(cls, member) {
    const lines = [];
    lines.push("static napi_value " + cls.tsName + "_" + member.getter + "_getter(napi_env env, napi_callback_info info)");
    lines.push("{");
    lines.push(INDENT + "napi_value jsThis = nullptr;");
    lines.push(INDENT + "SPURV_NAPI_CALL(napi_get_cb_info(env, info, nullptr, nullptr, &jsThis, nullptr));");
    lines.push(INDENT + cls.cppName + "* self = spurv::binding::unwrap<" + cls.cppName + ">(env, jsThis);");
    lines.push(INDENT + "if (spurv::binding::hasException(env)) {");
    lines.push(INDENT + "    return nullptr;");
    lines.push(INDENT + "}");
    emitReturn(member.type, "self->" + member.getter + "()").forEach((l) => lines.push(INDENT + l));
    lines.push("}");
    return lines;
}

function emitSetter(cls, member) {
    const lines = [];
    lines.push("static napi_value " + cls.tsName + "_" + member.setter + "(napi_env env, napi_callback_info info)");
    lines.push("{");
    lines.push(INDENT + "std::size_t argc = 1;");
    lines.push(INDENT + "napi_value args[1];");
    lines.push(INDENT + "napi_value jsThis = nullptr;");
    lines.push(INDENT + "SPURV_NAPI_CALL(napi_get_cb_info(env, info, &argc, args, &jsThis, nullptr));");
    lines.push(INDENT + "if (argc < 1) {");
    lines.push(INDENT + "    spurv::binding::throwArgCount(env, 1);");
    lines.push(INDENT + "    return nullptr;");
    lines.push(INDENT + "}");
    lines.push(INDENT + cls.cppName + "* self = spurv::binding::unwrap<" + cls.cppName + ">(env, jsThis);");
    lines.push(INDENT + "if (spurv::binding::hasException(env)) {");
    lines.push(INDENT + "    return nullptr;");
    lines.push(INDENT + "}");
    const res = emitParam(member.type, "value", member.type.optional === true, 0);
    res.lines.forEach((l) => lines.push(INDENT + l));
    lines.push(INDENT + "if (spurv::binding::hasException(env)) {");
    lines.push(INDENT + "    return nullptr;");
    lines.push(INDENT + "}");
    lines.push(INDENT + "self->" + member.setter + "(" + res.expr + ");");
    lines.push(INDENT + "return spurv::binding::undefined(env);");
    lines.push("}");
    return lines;
}

function emitMethod(cls, member) {
    const lines = [];
    const required = member.params.filter((p) => p.optional !== true).length;
    const n = member.params.length;
    const name = cls.tsName + "_" + member.cppName + "_method";
    lines.push("static napi_value " + name + "(napi_env env, napi_callback_info info)");
    lines.push("{");
    if (n === 0) {
        lines.push(INDENT + "napi_value jsThis = nullptr;");
        lines.push(INDENT + "SPURV_NAPI_CALL(napi_get_cb_info(env, info, nullptr, nullptr, &jsThis, nullptr));");
    } else {
        lines.push(INDENT + "std::size_t argc = " + n + ";");
        lines.push(INDENT + "napi_value args[" + n + "];");
        lines.push(INDENT + "napi_value jsThis = nullptr;");
        lines.push(INDENT + "SPURV_NAPI_CALL(napi_get_cb_info(env, info, &argc, args, &jsThis, nullptr));");
        lines.push(INDENT + "if (argc < " + required + ") {");
        lines.push(INDENT + "    spurv::binding::throwArgCount(env, " + required + ");");
        lines.push(INDENT + "    return nullptr;");
        lines.push(INDENT + "}");
    }
    lines.push(INDENT + cls.cppName + "* self = spurv::binding::unwrap<" + cls.cppName + ">(env, jsThis);");
    lines.push(INDENT + "if (spurv::binding::hasException(env)) {");
    lines.push(INDENT + "    return nullptr;");
    lines.push(INDENT + "}");
    const exprs = paramLinesAndExprs(member.params, lines);
    lines.push(INDENT + "if (spurv::binding::hasException(env)) {");
    lines.push(INDENT + "    return nullptr;");
    lines.push(INDENT + "}");
    const call = "self->" + member.cppName + "(" + exprs.join(", ") + ")";
    if (member.ret.tag === "void") {
        lines.push(INDENT + call + ";");
        lines.push(INDENT + "return spurv::binding::undefined(env);");
    } else {
        emitReturn(member.ret, call).forEach((l) => lines.push(INDENT + l));
    }
    lines.push("}");
    return lines;
}

function emitFunction(fn) {
    const lines = [];
    const required = fn.params.filter((p) => p.optional !== true).length;
    const n = fn.params.length;
    lines.push("static napi_value Function_" + fn.tsName + "(napi_env env, napi_callback_info info)");
    lines.push("{");
    if (n === 0) {
        lines.push(INDENT + "SPURV_NAPI_CALL(napi_get_cb_info(env, info, nullptr, nullptr, nullptr, nullptr));");
    } else {
        lines.push(INDENT + "std::size_t argc = " + n + ";");
        lines.push(INDENT + "napi_value args[" + n + "];");
        lines.push(INDENT + "SPURV_NAPI_CALL(napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));");
        lines.push(INDENT + "if (argc < " + required + ") {");
        lines.push(INDENT + "    spurv::binding::throwArgCount(env, " + required + ");");
        lines.push(INDENT + "    return nullptr;");
        lines.push(INDENT + "}");
    }
    const exprs = paramLinesAndExprs(fn.params, lines);
    lines.push(INDENT + "if (spurv::binding::hasException(env)) {");
    lines.push(INDENT + "    return nullptr;");
    lines.push(INDENT + "}");
    const call = fn.cppName + "(" + exprs.join(", ") + ")";
    if (fn.ret.tag === "void") {
        lines.push(INDENT + call + ";");
        lines.push(INDENT + "return spurv::binding::undefined(env);");
    } else {
        emitReturn(fn.ret, call).forEach((l) => lines.push(INDENT + l));
    }
    lines.push("}");
    return lines;
}

function emitPrototypeProps(cls) {
    const lines = [];
    lines.push("static const napi_property_descriptor " + cls.tsName + "_prototype_props[] = {");
    for (const member of cls.members) {
        if (member.kind === "property") {
            const getterFn = cls.tsName + "_" + member.getter + "_getter";
            const setterFn = member.setter !== undefined ? cls.tsName + "_" + member.setter : "nullptr";
            lines.push(
                INDENT + '{ .utf8name = "' + member.tsName + '", .method = nullptr, .getter = ' + getterFn +
                    ", .setter = " + setterFn + ", .value = nullptr, .attributes = napi_default, .data = nullptr },"
            );
        } else {
            lines.push(
                INDENT + '{ .utf8name = "' + member.tsName + '", .method = ' + cls.tsName + "_" + member.cppName +
                    "_method, .getter = nullptr, .setter = nullptr, .value = nullptr, .attributes = napi_default, .data = nullptr },"
            );
        }
    }
    lines.push("};");
    return lines;
}

function emitInitialize(model) {
    const lines = [];
    lines.push("napi_value initialize(napi_env env, napi_value exports)");
    lines.push("{");
    lines.push(INDENT + "napi_value ctor = nullptr;");
    lines.push("");
    for (const cls of model.classes) {
        lines.push(INDENT + "// class " + cls.tsName);
        const count = cls.members.length;
        const props = count > 0 ? cls.tsName + "_prototype_props" : "nullptr";
        lines.push(
            INDENT + "SPURV_NAPI_CALL(napi_define_class(env, \"" + cls.tsName + "\", NAPI_AUTO_LENGTH, " +
                cls.tsName + "_constructor, nullptr, " + count + ", " + props + ", &ctor));"
        );
        lines.push(INDENT + "SPURV_NAPI_CALL(napi_create_reference(env, ctor, 1, &g_" + cls.tsName + "_ctor));");
        lines.push(INDENT + 'SPURV_NAPI_CALL(napi_set_named_property(env, exports, "' + cls.tsName + '", ctor));');
        lines.push("");
    }
    for (const en of model.enums) {
        lines.push(INDENT + 'SPURV_NAPI_CALL(napi_set_named_property(env, exports, "' + en.name + '", Init_' + en.name + "(env)));");
    }
    for (const fn of model.functions) {
        lines.push(INDENT + "napi_value " + fn.tsName + "_fn = nullptr;");
        lines.push(
            INDENT + "SPURV_NAPI_CALL(napi_create_function(env, \"" + fn.tsName + "\", NAPI_AUTO_LENGTH, Function_" +
                fn.tsName + ", nullptr, &" + fn.tsName + "_fn));"
        );
        lines.push(INDENT + 'SPURV_NAPI_CALL(napi_set_named_property(env, exports, "' + fn.tsName + '", ' + fn.tsName + "_fn));");
    }
    lines.push(INDENT + "return exports;");
    lines.push("}");
    return lines;
}

function emitAll(model, dtsFiles) {
    const lines = [];
    lines.push("// GENERATED FILE -- DO NOT EDIT");
    lines.push("// generated by src/binding/generate.js (typescript " + ts.version + ")");
    for (const f of dtsFiles) {
        lines.push("//   from: " + f);
    }
    lines.push("");
    lines.push('#include "bindings.h"');
    lines.push('#include "BindingSupport.h"');
    const includes = [];
    for (const cls of model.classes) {
        const header = cls.header !== undefined ? cls.header : cls.tsName + ".h";
        includes.push("#include <" + header + ">");
    }
    includes.sort();
    for (const inc of includes) {
        lines.push(inc);
    }
    lines.push("");
    lines.push("#include <cstddef>");
    lines.push("#include <cstdint>");
    lines.push("#include <memory>");
    lines.push("#include <string>");
    lines.push("");
    lines.push("namespace spurv {");
    lines.push("namespace binding {");
    lines.push("namespace {");
    lines.push("");
    for (const cls of model.classes) {
        lines.push("[[maybe_unused]] static napi_value " + cls.tsName + "_wrapBorrowed(napi_env env, " + cls.cppName + "* ptr);");
    }
    lines.push("");
    for (const cls of model.classes) {
        lines.push("static napi_ref g_" + cls.tsName + "_ctor = nullptr;");
    }
    lines.push("");
    for (const en of model.enums) {
        emitEnumInit(en).forEach((l) => lines.push(l));
        lines.push("");
    }
    for (const cls of model.classes) {
        if (cls.hasCtor) {
            emitClassCtor(cls).forEach((l) => lines.push(l));
            lines.push("");
        }
        for (const member of cls.members) {
            if (member.kind === "property") {
                emitGetter(cls, member).forEach((l) => lines.push(l));
                lines.push("");
                if (member.setter !== undefined) {
                    emitSetter(cls, member).forEach((l) => lines.push(l));
                    lines.push("");
                }
            } else {
                emitMethod(cls, member).forEach((l) => lines.push(l));
                lines.push("");
            }
        }
        emitWrapBorrowed(cls).forEach((l) => lines.push(l));
        lines.push("");
    }
    for (const fn of model.functions) {
        emitFunction(fn).forEach((l) => lines.push(l));
        lines.push("");
    }
    for (const cls of model.classes) {
        if (cls.members.length > 0) {
            emitPrototypeProps(cls).forEach((l) => lines.push(l));
            lines.push("");
        }
    }
    lines.push("} // namespace");
    lines.push("");
    emitInitialize(model).forEach((l) => lines.push(l));
    lines.push("");
    lines.push("} // namespace binding");
    lines.push("} // namespace spurv");
    lines.push("");
    return lines.join("\n");
}

// ---------------------------------------------------------------------------
// main

const args = parseArgs(process.argv.slice(2));

let tsconfig = {};
try {
    tsconfig = require(path.join(process.cwd(), "tsconfig.json"));
} catch (e) {
    // no local tsconfig
}
const options = Object.assign({ target: ts.ScriptTarget.ES2022 }, tsconfig.compilerOptions);

const program = ts.createProgram(args.dts, options);
const checker = program.getTypeChecker();

const model = { enums: [], classes: [], functions: [], warned: [] };
for (const file of args.dts) {
    const sourceFile = program.getSourceFile(file);
    if (sourceFile === undefined) {
        console.error("generate.js: cannot read " + file);
        process.exit(1);
    }
    collect(model, checker, sourceFile);
}

const header = '#pragma once\n\n#include <node_api.h>\n\nnamespace spurv {\nnamespace binding {\n\nnapi_value initialize(napi_env env, napi_value exports);\n\n} // namespace binding\n} // namespace spurv\n';

fs.mkdirSync(args.out, { recursive: true });
fs.writeFileSync(path.join(args.out, "bindings.h"), header);
fs.writeFileSync(path.join(args.out, "bindings.cpp"), emitAll(model, args.dts));

for (const w of model.warned) {
    console.error("generate.js: " + w);
}
console.error(
    "generate.js: wrote " + model.enums.length + " enums, " + model.classes.length + " classes, " +
        model.functions.length + " functions to " + args.out
);
