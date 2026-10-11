#!/usr/bin/env python3
"""Static check of game scripts against the engine's script API.

Compares the names used in a directory of *.script files with the bindings dump
the engine writes with -dump_bindings (scriptbindings_N.txt in $app_data_root$,
use the largest one). Reports:

  - constants:  ENGINE_TABLE.NAME where the table is exported but NAME is not
                (these evaluate to nil silently)
  - namespace:  ns.func(...) where ns is an exported namespace without func
  - methods:    obj:method(...) on game_object-like receivers where game_object
                has no such method
  - globals:    bare calls that are defined neither by the engine, nor by Lua,
                nor by any script

It is a heuristic: results are candidates to check by hand or at runtime.

Usage: find_missing_exports.py <bindings dump> <scripts dir> [--markdown]
"""

import re
import sys
from collections import defaultdict
from pathlib import Path

LUA_GLOBALS = set("""
assert collectgarbage dofile error getfenv getmetatable ipairs load loadfile loadstring
module next pairs pcall print rawequal rawget rawset require select setfenv setmetatable
tonumber tostring type unpack xpcall gcinfo newproxy
string table math io os debug coroutine package bit jit ffi
class super
""".split())

# Receivers that hold a game_object in GSC scripts
GAME_OBJECT_RECEIVERS = re.compile(
    r"(?:\b(?:npc|obj|object|actor|victim|who|killer|enemy|target|first_speaker|second_speaker|"
    r"speaker|talker|item|stalker|self\.object|db\.actor)|level\.object_by_id\([^)]*\))"
    r":([A-Za-z_]\w*)\s*\(")


def parse_bindings(path):
    """Return (globals, scopes) where scopes maps a class/namespace name to
    {'members': set, 'consts': set, 'bases': list}."""
    scopes = defaultdict(lambda: {"members": set(), "consts": set(), "bases": []})
    global_names = set()
    stack = []
    pending = None
    decl = re.compile(r"([A-Za-z_]\w*)\s*\(")
    const = re.compile(r"const\s+\w+\s+([A-Za-z_]\w*)\s*=")
    for raw in Path(path).read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        m = re.match(r"(class|namespace)\s+([A-Za-z_]\w*)(?:\s*:\s*(.*))?$", line)
        if m:
            pending = m.group(2)
            if m.group(3):
                scopes[pending]["bases"] = [b.strip() for b in m.group(3).split(",")]
            else:
                scopes[pending]  # create
            continue
        if line == "{":
            stack.append(pending)
            pending = None
            continue
        if line.startswith("}"):
            if stack:
                stack.pop()
            continue
        target = scopes[stack[-1]] if stack else None
        c = const.match(line)
        if c:
            (target["consts"] if target is not None else global_names).add(c.group(1))
            continue
        d = decl.findall(line)
        if d and line.endswith(";"):
            # the declared name is the identifier right before the first '(' of the parameter list
            name = re.search(r"([A-Za-z_]\w*|operator\S*)\s*\((?!.*\(.*\)\s*\()", line)
            name = d[0] if not name else name.group(1)
            # constructors are listed as "__init(...)"; skip return-type identifiers
            (target["members"] if target is not None else global_names).add(name)
    return global_names, scopes


def all_members(scopes, cls, seen=None):
    seen = seen or set()
    if cls in seen or cls not in scopes:
        return set()
    seen.add(cls)
    out = set(scopes[cls]["members"]) | set(scopes[cls]["consts"])
    for base in scopes[cls]["bases"]:
        out |= all_members(scopes, base, seen)
    return out


def strip_comments_and_strings(text):
    text = re.sub(r"--\[(=*)\[.*?\]\1\]", "", text, flags=re.S)
    text = re.sub(r"--[^\n]*", "", text)
    text = re.sub(r"\[(=*)\[.*?\]\1\]", '""', text, flags=re.S)
    text = re.sub(r'"(?:\\.|[^"\\\n])*"', '""', text)
    text = re.sub(r"'(?:\\.|[^'\\\n])*'", "''", text)
    return text


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    globals_, scopes = parse_bindings(sys.argv[1])
    scripts = {p.stem: strip_comments_and_strings(p.read_bytes().decode("cp1251", errors="replace"))
               for p in sorted(Path(sys.argv[2]).glob("*.script"))}

    # Names defined by scripts: global functions, module functions, methods, assignments
    defined_anywhere = set()
    defined_in = defaultdict(set)
    for mod, text in scripts.items():
        for m in re.finditer(r"\bfunction\s+([A-Za-z_][\w.:]*)\s*\(", text):
            full = m.group(1)
            last = re.split(r"[.:]", full)[-1]
            defined_anywhere.add(last)
            defined_in[mod].add(full.split(".")[0].split(":")[0])
            defined_in[mod].add(last)
        for m in re.finditer(r"(?:^|\n)\s*(?:local\s+)?([A-Za-z_][\w.]*)\s*=", text):
            defined_in[mod].add(m.group(1).split(".")[-1])
            defined_in[mod].add(m.group(1).split(".")[0])
        for m in re.finditer(r"\blocal\s+([\w\s,]+)", text):
            for n in re.split(r"[\s,]+", m.group(1)):
                if n:
                    defined_in[mod].add(n)
        for m in re.finditer(r"\bfunction\s*[\w.:]*\s*\(([^)]*)\)", text):
            for n in re.split(r"[\s,]+", m.group(1)):
                if n and n != "...":
                    defined_in[mod].add(n)
        for m in re.finditer(r"\bfor\s+([\w\s,]+?)\s+(?:in|=)", text):
            for n in re.split(r"[\s,]+", m.group(1)):
                if n:
                    defined_in[mod].add(n)
        for m in re.finditer(r"\bclass\s+\"(\w+)\"", text):
            defined_anywhere.add(m.group(1))
            defined_in[mod].add(m.group(1))
    g_defined = defined_in.get("_g", set())

    findings = defaultdict(lambda: defaultdict(set))  # kind -> name -> {modules}

    engine_tables = {name for name, s in scopes.items() if s["consts"] or s["members"]}
    game_object = all_members(scopes, "game_object")

    for mod, text in scripts.items():
        local_names = defined_in[mod]
        # TABLE.NAME on engine tables (constants and namespace functions)
        for m in re.finditer(r"(?<![\w.:])([A-Za-z_]\w*)\.([A-Za-z_]\w*)(\s*\()?", text):
            table, name, call = m.group(1), m.group(2), m.group(3)
            if table in scripts or table in local_names or table in g_defined:
                continue
            if table not in engine_tables:
                continue
            if name in all_members(scopes, table):
                continue
            kind = "namespace" if call else "constants"
            findings[kind][f"{table}.{name}"].add(mod)
        # obj:method( on game_object receivers
        for m in GAME_OBJECT_RECEIVERS.finditer(text):
            name = m.group(1)
            if name not in game_object and name not in defined_anywhere:
                findings["methods"][f"game_object:{name}"].add(mod)
        # bare global calls
        for m in re.finditer(r"(?<![\w.:])([A-Za-z_]\w*)\s*\(", text):
            name = m.group(1)
            if name in ("function", "if", "while", "until", "return", "and", "or", "not", "elseif", "in"):
                continue
            if (name in LUA_GLOBALS or name in globals_ or name in scopes or name in local_names
                    or name in g_defined or name in scripts or name in defined_anywhere):
                continue
            findings["globals"][name].add(mod)

    md = "--markdown" in sys.argv
    for kind in ("constants", "namespace", "methods", "globals"):
        items = findings.get(kind, {})
        if md:
            print(f"\n### {kind} ({len(items)})\n")
            print("| Name | Used in |\n|---|---|")
        else:
            print(f"== {kind} ({len(items)})")
        for name in sorted(items):
            mods = sorted(items[name])
            where = ", ".join(mods[:6]) + (f" (+{len(mods) - 6})" if len(mods) > 6 else "")
            print(f"| `{name}` | {where} |" if md else f"  {name:50} {where}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
