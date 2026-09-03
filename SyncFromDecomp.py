#!/usr/bin/env python3
"""
Generate CGecko-usable game headers from the MSSB decomp repo.

    python SyncFromDecomp.py

Decomp is a git submodule at ./Decomp; pass --decomp to point elsewhere.

Mirrors the decomp's own `include/` layout into `Include/`, so an include line
reads like it does in the decomp:

    #include "Include/game/UnknownHomes_Game.h"

Hand-written headers live alongside it in `Include/Rio/` and are never touched.

Types, enums and macros are copied through unchanged. Declarations are rewritten
so the compiler knows where each symbol lives:

    extern InMemBallType g_Ball;
        -> #define g_Ball VAR_ADDRESS(InMemBallType, 0x80890B38)
    void manageLoadingState(void);
        -> static inline void manageLoadingState(void) { ((void(*)(void))0x8063F6F0)(); }

Symbols are matched to addresses **by name** against the decomp's
`config/GYQE01/**/symbols.txt` (85% of prototypes and 62% of extern data resolve).
Anything unresolved is commented out and listed in the sync report rather than
silently dropped.

Output is generated -- do not hand-edit, and it is gitignored. Hand-written
headers (Rio helpers, mid-function hook sites, Project Rio's own variables)
belong in `Include/Rio/`, which this script never touches.
"""
import argparse
import importlib.util
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT_ROOT = HERE / "Include"
REPORT = OUT_ROOT / "_sync_report.txt"

# Everything under Include/ that this script owns and will overwrite. Hand-written
# headers live in Include/Rio/ and are never touched.
GENERATED_ENTRIES = ("game", "static", "menus", "unused_rel", "Symbols", "Dolphin", "musyx",
                     "C3", "charPipeline", "PowerPC_EABI_Support", "stl", "text",
                     "Unknown", "mssbTypes.h", "types.h", "BuildSettings.h",
                     "header_rep_data.h", "executor.h", "_sync_report.txt")

# Roots to generate from; everything they #include comes along transitively.
# The SDK trees are included because mods call into them directly (DVDFastOpen,
# CARDProbeEx, GXSetProjection, PSMTXTrans, sndFXStartEx, ...) -- without them
# those symbols have no address and the mod cannot be ported off the Ghidra
# export.
ROOT_DIRS = ("game", "static", "menus", "unused_rel", "text", "Unknown",
             "Dolphin", "musyx", "C3", "charPipeline", "PowerPC_EABI_Support")
ROOT_FILES = ("mssbTypes.h", "types.h")

# Macros CGecko's Common.h owns. The decomp defines its own versions; letting
# those win would break mods -- notably `ASM`, which is an empty macro in the
# decomp but a function-like macro mods actually call in CGecko.
CGECKO_OWNED_MACROS = {"ASM", "OFFSET_OF", "true", "false"}
MACRO_DEF_RE = re.compile(r"^\s*#\s*define\s+(\w+)")

# Typedefs the devkitPPC toolchain already provides via <stddef.h>, with widths
# that do not match the decomp's (`u32 size_t` vs the compiler's `unsigned int`,
# `u16 wchar_t` vs `long int`). Redefining them is a hard error.
TOOLCHAIN_OWNED_TYPEDEFS = {"size_t", "wchar_t", "ptrdiff_t"}
TYPEDEF_RE = re.compile(r"^\s*typedef\s+[\w\s\*]+?\b(\w+)\s*;\s*$")


# --------------------------------------------------------------------------
# symbol table (borrowed from the decomp so the two never drift)
# --------------------------------------------------------------------------
def load_decomp_module(decomp: Path, name: str):
    path = decomp / "tools" / (name + ".py")
    if not path.exists():
        sys.exit("not a decomp checkout (missing %s)" % path)
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.path.insert(0, str(decomp / "tools"))
    spec.loader.exec_module(mod)
    return mod


CONFIG_RE = re.compile(r"^\s*(\w+)\s*=\s*\.(\w+):0x([0-9A-Fa-f]+);(.*)$")


def build_symbol_table(decomp: Path, cvt):
    """{name: (addr, kind)} for the retail build. DOL addresses are absolute;
    REL addresses are section-relative and get rebased."""
    syms = {}
    build = decomp / "config" / "GYQE01"
    for p in sorted(build.rglob("symbols.txt")):
        rel = p.parent.name if p.parent.name != "GYQE01" else None
        bases = None
        if rel:
            try:
                bases = cvt.section_bases(rel)
            except KeyError:
                continue  # REL whose load address is unsolved -- skip, don't guess
        for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
            m = CONFIG_RE.match(line)
            if not m:
                continue
            name, section, addr, meta = m.groups()
            a = int(addr, 16)
            if rel:
                base = bases.get(section)
                if base is None:
                    continue
                a += base
            kind = "func" if "type:function" in meta else "data"
            syms.setdefault(name, (a, kind))
    return syms


# --------------------------------------------------------------------------
# raw address bindings for symbols no decomp header declares
# --------------------------------------------------------------------------
# A symbol only reaches a mod if some decomp header declares it, and most do
# not: the decomp knows a name and an address for thousands of functions and
# objects that no header mentions, and those were invisible to modders even
# though the address was sitting in symbols.txt the whole time. This pass emits
# them so nothing the decomp knows about is unreachable.
#
# Only the address is emitted, never a type. The decomp records a name, an
# address and a size for these -- it does not record what they look like, and
# asserting `u32` on something that is really a float is the kind of guess that
# reads like a fact. The type is named at the point of use, which is what
# CGecko's macros take anyway.

PLACEHOLDER_SYM = re.compile(
    r"^(fn_\d+_[0-9A-Fa-f]+|fn_[0-9A-Fa-f]{6,}|lbl_|auto_|pad_|@|__?\d)")

IDENT_RE = re.compile(r"[A-Za-z_]\w*")

# game and menus occupy the same arena slot and are mutually exclusive, so their
# bindings must never be in scope at the same time.
CONTEXT_GUARD = """#if defined(MSSB_CONTEXT_%(other)s)
#error "the %(mine_l)s and %(other_l)s RELs share one arena slot -- a mod cannot bind both"
#endif
#ifndef MSSB_CONTEXT_%(mine)s
#define MSSB_CONTEXT_%(mine)s 1
#endif
"""

SYM_PRELUDE = """// ==========================================================================
//  AUTO-GENERATED by SyncFromDecomp.py. DO NOT HAND-EDIT.
//
//  Address bindings for every hand-named symbol in the decomp's
//  config/GYQE01/**/symbols.txt that no decomp header declares.
//  %(what)s
//
//  Only the address is given -- the decomp knows where these live, not what
//  they look like -- so name the type at the point of use:
//
//      VAR_ADDRESS(float, someValue_ADDR) = 1.f;
//      MyStruct* p = (MyStruct*)someObject_ADDR;
//      FUNCTION_ADDRESS(int, someFunction_ADDR, int, float)(3, 1.f);
//
//  The trailing comment is the size the decomp recorded. Anything here that
//  later gains a real declaration in the decomp drops out of this file on the
//  next sync and shows up properly typed instead.
//
//  Addresses are never double-counted: a symbol whose address falls inside
//  another symbol's extent is not emitted, because a mod reaches it through the
//  containing object (x.a, not a's own address).
// ==========================================================================
#pragma once
#include "CGecko/Common.h"

"""

MODULE_BLURB = {
    "dol": "These live in the DOL, which is always resident.",
    "game": "These live in the match REL, resident only during a match.",
    "menus": "These live in the menu REL, resident only in the menus.",
}


def build_symbol_meta(decomp: Path, cvt):
    """{name: (addr, kind, size, module)} -- build_symbol_table, but keeping the
    recorded size and which module the symbol came from."""
    out = {}
    build = decomp / "config" / "GYQE01"
    for p in sorted(build.rglob("symbols.txt")):
        rel = p.parent.name if p.parent.name != "GYQE01" else None
        module = rel or "dol"
        bases = None
        if rel:
            try:
                bases = cvt.section_bases(rel)
            except KeyError:
                continue        # REL whose load address is unsolved -- never guess
        for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
            m = CONFIG_RE.match(line)
            if not m:
                continue
            name, section, addr, meta = m.groups()
            a = int(addr, 16)
            if rel:
                base = bases.get(section)
                if base is None:
                    continue
                a += base
            size = re.search(r"size:0x([0-9A-Fa-f]+)", meta)
            out.setdefault(name, (a,
                                  "func" if "type:function" in meta else "data",
                                  int(size.group(1), 16) if size else None,
                                  module))
    return out


def nested_symbols(meta):
    """Names whose address falls strictly inside another symbol's extent.

    A struct's members, or a function's interior labels, are reached through the
    thing that contains them -- a mod writes `x.a`, never `a`'s own address --
    so emitting both double-counts the same memory.

    Containment is judged per module, never across. The match and menu RELs are
    rebased into the same arena slot, so a game symbol and a menus symbol can
    hold identical addresses while having nothing to do with each other.

    Every symbol in the table is a candidate container, including ones the
    decomp declares properly: those are the most important containers of all,
    since that is the typed struct the mod is meant to go through.
    """
    by_mod = {}
    for name, (addr, kind, size, module) in meta.items():
        by_mod.setdefault(module, []).append((addr, size or 0, name))

    nested = set()
    for rows in by_mod.values():
        # widest first at a shared address, so an alias sitting on top of a real
        # object is the one dropped, not the object
        rows.sort(key=lambda r: (r[0], -r[1]))
        reach = None          # furthest end seen among symbols starting before this one
        for addr, size, name in rows:
            if reach is not None and addr < reach:
                nested.add(name)          # an earlier symbol still covers this address
            end = addr + size
            # only extend the frontier once this symbol can no longer be the
            # candidate itself, i.e. after testing it
            reach = end if reach is None else max(reach, end)
    return nested


def emit_symbol_headers(meta, already, dry_run):
    """Write Include/Symbols/<module>.h. `already` is every identifier appearing
    in the generated headers -- those are declared properly and must not be
    shadowed by a bare address."""
    nested = nested_symbols(meta)
    groups = {}
    for name, (addr, kind, size, module) in meta.items():
        if module not in MODULE_BLURB or name in already or PLACEHOLDER_SYM.match(name):
            continue
        if name in nested:
            continue          # reached through its container, not its own address
        groups.setdefault(module, []).append((name, addr, kind, size))

    written = {"_nested": sum(1 for n in nested
                              if n in meta and meta[n][3] in MODULE_BLURB
                              and n not in already and not PLACEHOLDER_SYM.match(n))}
    for module, rows in sorted(groups.items()):
        rows.sort(key=lambda r: (r[2] != "func", r[1]))
        body = [SYM_PRELUDE % {"what": MODULE_BLURB[module]}]
        if module in ("game", "menus"):
            other = "MENUS" if module == "game" else "GAME"
            body.append(CONTEXT_GUARD % {"mine": module.upper(),
                                         "mine_l": module,
                                         "other": other,
                                         "other_l": other.lower()} + "\n")
        for kind, label in (("func", "functions"), ("data", "data / variables")):
            sel = [r for r in rows if r[2] == kind]
            if not sel:
                continue
            body.append("/* ---- %s (%d) ---- */\n" % (label, len(sel)))
            width = max(len(r[0]) for r in sel) + 5
            for name, addr, _kind, size in sel:
                sz = ("  // size 0x%X" % size) if size else ""
                body.append("#define %-*s 0x%08X%s\n" % (width, name + "_ADDR", addr, sz))
            body.append("\n")
        written[module] = len(rows)
        if not dry_run:
            dst = OUT_ROOT / "Symbols" / (module + ".h")
            dst.parent.mkdir(parents=True, exist_ok=True)
            dst.write_text("".join(body), encoding="utf-8", newline="\n")
    return written


# --------------------------------------------------------------------------
# declaration rewriting
# --------------------------------------------------------------------------
EXTERN_RE = re.compile(
    r"^(?P<indent>\s*)extern\s+(?P<type>[A-Za-z_][\w\s\*]*?)\s*"
    r"(?P<name>\w+)\s*(?P<dims>(?:\[[^\]]*\])*)\s*;\s*$")

PROTO_RE = re.compile(
    r"^(?P<indent>\s*)(?P<ret>[A-Za-z_][\w\s\*]*?)\s*"
    r"(?P<name>\w+)\s*\((?P<params>[^;{]*)\)\s*;\s*$")

# Lines that look like a prototype but are not one.
NOT_PROTO = re.compile(r"^\s*(typedef|struct|union|enum|return|static|#|//|/\*)")

# `extern "C" {` wraps a whole SDK header. Counting its brace would put every
# declaration in the file at depth > 0, so they would all be passed through as
# bare prototypes with no address -- and, because they were never examined,
# they would not even show up as unresolved. Treat it as depth-neutral.
EXTERN_C_OPEN = re.compile(r'^\s*extern\s*"C"\s*\{\s*$')


def split_params(s):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


TYPE_KEYWORDS = {"int", "char", "short", "long", "float", "double", "void",
                 "unsigned", "signed", "const", "volatile", "struct", "union",
                 "enum", "bool"}

# Tokens that only ever qualify a type; they can never be the type themselves.
QUALIFIERS = {"const", "volatile", "struct", "union", "enum", "unsigned", "signed"}


def parse_param(p, idx):
    """-> (declaration, cast_type, argument_name)

    The decomp writes parameters both ways -- `VecXYZ* b` (named) and `u8`
    (type only). Splitting a single token like `u8` into type `u` + name `8`
    was the original bug here, so a lone token is always a bare type.
    """
    p = p.strip()
    if p in ("void", ""):
        return None
    if p == "...":
        return ("...", "...", None)

    # function-pointer parameter: `void (*func)(void)` -- the name sits inside
    # the declarator, so neither the last-token nor the array rule finds it
    m = re.match(r"^(?P<ret>.+?)\(\s*\*\s*(?P<name>\w*)\s*\)\s*(?P<args>\(.*\))$", p)
    if m:
        ret, name, args = m.group("ret").strip(), m.group("name") or "a%d" % idx, m.group("args")
        return ("%s (*%s)%s" % (ret, name, args), "%s (*)%s" % (ret, args), name)

    # array parameter: `int foo[4]` -- decays to a pointer in the cast
    m = re.match(r"^(?P<pre>.*?)(?P<name>\w+)\s*(?P<dims>(?:\[[^\]]*\])+)$", p)
    if m and m.group("pre").strip():
        typ = m.group("pre").strip()
        return ("%s %s%s" % (typ, m.group("name"), m.group("dims")),
                typ + "*", m.group("name"))

    toks = p.split()
    if len(toks) == 1:
        return (p, p, "a%d" % idx)          # bare type: `u8`, `VecXYZ*`

    last, stars = toks[-1], ""
    while last.startswith("*"):
        stars += "*"
        last = last[1:]
    if last in TYPE_KEYWORDS or not re.fullmatch(r"[A-Za-z_]\w*", last):
        return (p, p, "a%d" % idx)          # `unsigned int`, `const char*`
    # If everything before the last token is only qualifiers, the last token is
    # the type, not a parameter name -- `const Mtx` is a bare type, whereas
    # `int count` and `const Vec* v` really do name their parameter.
    if all(t in QUALIFIERS for t in toks[:-1]):
        return (p, p, "a%d" % idx)
    typ = " ".join(toks[:-1]) + stars
    return ("%s %s" % (typ, last), typ, last)


def rewrite_proto(m, addr):
    ret = re.sub(r"\s+", " ", m.group("ret")).strip()
    # the decomp puts `extern` on some prototypes; a wrapper must not inherit it
    ret = re.sub(r"^extern\s+", "", ret)
    name = m.group("name")
    raw = split_params(m.group("params"))
    parsed = [parse_param(p, i) for i, p in enumerate(raw)]
    parsed = [p for p in parsed if p is not None]

    if any(p[0] == "..." for p in parsed):
        casts = [p[1] for p in parsed]
        return ("#define %s FUNCTION_ADDRESS(%s, 0x%08X, %s)"
                % (name, ret, addr, ", ".join(casts)))

    if not parsed:
        sig, casts, args = "void", "void", ""
    else:
        decls, cast_list, names = [], [], []
        for i, (decl, cast, argname) in enumerate(parsed):
            argname = argname or ("a%d" % i)
            if not re.search(r"\b%s\b" % re.escape(argname), decl):
                decl = "%s %s" % (decl, argname)
            decls.append(decl)
            cast_list.append(cast)
            names.append(argname)
        sig, casts, args = ", ".join(decls), ", ".join(cast_list), ", ".join(names)

    call = "((%s(*)(%s))0x%08X)(%s);" % (ret, casts, addr, args)
    body = call if ret == "void" else ("return " + call)
    return "static inline %s %s(%s) { %s }" % (ret, name, sig, body)


def rewrite_extern(m, addr):
    typ = re.sub(r"\s+", " ", m.group("type")).strip()
    name, dims = m.group("name"), m.group("dims")
    if not dims:
        return "#define %s VAR_ADDRESS(%s, 0x%08X)" % (name, typ, addr)
    inner = re.findall(r"\[([^\]]*)\]", dims)
    if any(d.strip() == "" for d in inner):  # `extern T x[];` -- unknown extent
        return "#define %s VAR_ADDRESS(%s, 0x%08X)" % (name, typ + "*", addr)
    n = len(inner)
    if n > 5:
        return None
    return "#define %s ARRAY_%dD_ADDRESS(%s, %s, 0x%08X)" % (
        name, n, typ, ", ".join(d.strip() for d in inner), addr)


def collect_declared_functions(sources, inc):
    """{function name: how many headers declare it}.

    The SDK declares some functions in more than one header (PSVECAdd is in both
    mtx.h and vec.h). Duplicate *prototypes* are legal C; duplicate `static
    inline` definitions are not, so those few need an include guard."""
    counts = {}
    for src in sources:
        depth = 0
        for line in src.read_text(encoding="utf-8", errors="replace").splitlines():
            if EXTERN_C_OPEN.match(line):
                continue
            if depth > 0:
                depth = max(0, depth + line.count("{") - line.count("}"))
                continue
            stripped = line.split("//")[0]
            if not NOT_PROTO.match(line):
                m = PROTO_RE.match(line)
                if m:
                    counts[m.group("name")] = counts.get(m.group("name"), 0) + 1
                    continue
            depth = max(0, depth + stripped.count("{") - stripped.count("}"))
    return {n for n, c in counts.items() if c > 1}


# ---- layout guards ----------------------------------------------------------
# The decomp annotates struct members with their offset (`/*0x0C*/ u8 x;`) and
# the closing typedef with the size (`} Name; // size: 0x58`). Those numbers are
# what the decomp was matched against, and CodeWarrior and powerpc-eabi-gcc do
# not always agree on layout. Emitting them as _Static_asserts turns a silent
# wrong-offset read into a build error in the mod that includes the header.
LAYOUT_ASSERTS = True
# Structs whose decomp offset/size comments are known not to match what any C
# compiler produces (e.g. a `size: 0x22` on a struct holding f32s). They are
# listed in the sync report instead of asserted, until the decomp settles them.
LAYOUT_ASSERT_SKIP = {
    "UnkStarDashFloatStruct",   # eight f32 + s16: C pads to 0x24, comment says 0x22
    "MiniGameStruct",           # embeds SomeStarDashStruct (holds an f32) at 2-aligned offsets
    "SomeStarDashStruct",       # same cluster; its offsets only make sense unaligned
    "StatTable",                # 0x3B bytes in the game; holds a u32, so C makes it 0x3C
    "CharacterStats",           # therefore chemistry sits at 0x3C in C but 0x3B in the game
}
STRUCT_OPEN_RE = re.compile(r"^\s*(?:typedef\s+)?struct(?:\s+\w+)?\s*\{\s*$")
MEMBER_RE = re.compile(r"^\s*/\*\s*0x([0-9A-Fa-f]+)\s*\*/\s*(?P<decl>[^;]+);")
STRUCT_CLOSE_RE = re.compile(r"^\s*\}\s*(?P<name>\w*)\s*;(?:.*?size:?\s*0x(?P<size>[0-9A-Fa-f]+))?")
MEMBER_NAME_RE = re.compile(r"(\w+)\s*(?:\[[^\]]*\]\s*)*$")


def member_name(decl):
    """`E(u8, STADIUM_ID) StadiumID` / `s16 _20[4][2]` / `struct Foo *p` -> name.
    None for anything this cannot read safely (function pointers, bitfields)."""
    d = decl.split("//")[0].strip()
    if ":" in d or "(*" in d:
        return None
    d = re.sub(r"E\([^)]*\)", "", d)          # drop the enum-annotation macro
    m = MEMBER_NAME_RE.search(d)
    if not m or m.group(1) in TYPE_KEYWORDS:
        return None
    return m.group(1)


def layout_asserts(name, members, size):
    lines = []
    for off, mem in members:
        lines.append("_Static_assert(offsetof(%s, %s) == 0x%X, \"%s.%s is not at 0x%X\");"
                     % (name, mem, off, name, mem, off))
    if size is not None:
        lines.append("_Static_assert(sizeof(%s) == 0x%X, \"sizeof(%s) is not 0x%X\");"
                     % (name, size, name, size))
    return lines


def rewrite_header(text, syms, stats, unresolved, relpath, dupes=frozenset()):
    out = []
    depth = 0
    struct_members, struct_name, want_asserts = [], None, False
    for line in text.splitlines():
        # Only file-scope declarations are rewritten. Inside a struct body a
        # line like `artificial_padding(0x15, 0x18, u8);` looks exactly like a
        # prototype, and commenting it out would silently change the layout.
        if EXTERN_C_OPEN.match(line):
            out.append(line)
            continue
        if depth > 0:
            out.append(line)
            new_depth = max(0, depth + line.count("{") - line.count("}"))
            if want_asserts and depth == 1:
                mm = MEMBER_RE.match(line)
                if mm and new_depth == 1:
                    nm = member_name(mm.group("decl"))
                    if nm:
                        struct_members.append((int(mm.group(1), 16), nm))
                if new_depth == 0:
                    mc = STRUCT_CLOSE_RE.match(line)
                    name = (mc.group("name") if mc else "") or struct_name
                    size = int(mc.group("size"), 16) if mc and mc.group("size") else None
                    if name in LAYOUT_ASSERT_SKIP:
                        out.append("// [layout asserts skipped: decomp comments known inconsistent] " + name)
                        unresolved.append((relpath, "layout", name))
                    elif name and (struct_members or size is not None):
                        out.extend(layout_asserts(name, struct_members, size))
                        stats["layout_asserts"] += len(struct_members) + (size is not None)
                    struct_members, struct_name, want_asserts = [], None, False
            depth = new_depth
            continue
        if LAYOUT_ASSERTS and STRUCT_OPEN_RE.match(line):
            tag = re.search(r"struct\s+(\w+)\s*\{", line)
            struct_name = ("struct " + tag.group(1)) if tag else None
            struct_members, want_asserts = [], True
        md = MACRO_DEF_RE.match(line)
        if md and md.group(1) in CGECKO_OWNED_MACROS:
            out.append("// [CGecko owns this macro] " + line.strip())
            stats["macros_suppressed"] += 1
            continue
        td = TYPEDEF_RE.match(line)
        if td and td.group(1) in TOOLCHAIN_OWNED_TYPEDEFS:
            out.append("// [toolchain owns this typedef] " + line.strip())
            stats["macros_suppressed"] += 1
            continue
        stripped = line.split("//")[0]
        m = EXTERN_RE.match(line)
        if m and m.group("name") in syms and syms[m.group("name")][1] == "data":
            rep = rewrite_extern(m, syms[m.group("name")][0])
            if rep:
                out.append(m.group("indent") + rep)
                stats["data_bound"] += 1
                continue
        if m:
            unresolved.append((relpath, "data", m.group("name")))
            out.append("// [unresolved] " + line.strip())
            stats["data_unresolved"] += 1
            continue

        if not NOT_PROTO.match(line):
            m = PROTO_RE.match(line)
            if m:
                name = m.group("name")
                if name in syms and syms[name][1] == "func":
                    wrapper = m.group("indent") + rewrite_proto(m, syms[name][0])
                    if name in dupes:
                        guard = "CGECKO_FN_" + name
                        wrapper = "\n".join(
                            ["#ifndef " + guard, "#define " + guard, wrapper, "#endif"])
                    out.append(wrapper)
                    stats["func_bound"] += 1
                else:
                    unresolved.append((relpath, "func", name))
                    out.append("// [unresolved] " + line.strip())
                    stats["func_unresolved"] += 1
                continue
        out.append(line)
        # clamp: the `}` closing an `extern "C" {` would otherwise go negative
        depth = max(0, depth + stripped.count("{") - stripped.count("}"))
    return "\n".join(out) + "\n"


INCLUDE_RE = re.compile(r'^(\s*#\s*include\s*)"([^"]+)"(.*)$')


def rewrite_includes(text, copied):
    def sub(m):
        path = m.group(2)
        if path in copied:
            return '%s"Include/%s"%s' % (m.group(1), path, m.group(3))
        return "// [skipped include] " + m.group(0).strip()
    return "\n".join(INCLUDE_RE.sub(sub, ln) if INCLUDE_RE.match(ln) else ln
                     for ln in text.splitlines()) + "\n"


PRELUDE = """// ==========================================================================
//  AUTO-GENERATED by SyncFromDecomp.py from the MSSB decomp. DO NOT HAND-EDIT.
//  Source: {src}
// ==========================================================================
#pragma once
#include <stddef.h>   // size_t/wchar_t: the toolchain's widths, not the decomp's
#include "CGecko/Common.h"
"""


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--decomp", default=str(HERE / "Decomp"))
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--no-layout-asserts", action="store_true",
                    help="do not emit _Static_assert layout guards from the decomp's offset/size comments")
    args = ap.parse_args()
    global LAYOUT_ASSERTS
    LAYOUT_ASSERTS = not args.no_layout_asserts

    decomp = Path(args.decomp).expanduser().resolve()
    if not (decomp / "include").is_dir():
        sys.exit("no include/ under %s" % decomp)

    cvt = load_decomp_module(decomp, "cvt_rel_addr_to_mapped_addr")
    syms = build_symbol_table(decomp, cvt)
    print("symbol table: %d names" % len(syms))

    inc = decomp / "include"
    roots = [n for n in ROOT_FILES if (inc / n).exists()]
    for d in ROOT_DIRS:
        if (inc / d).is_dir():
            roots += [p.relative_to(inc).as_posix() for p in (inc / d).rglob("*.h")]

    # transitive #include closure, so things like Dolphin/pad.h (PAD_BUTTON_*)
    # come along without vendoring the whole SDK
    inc_re = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.M)
    seen, queue, missing = set(), list(roots), set()
    while queue:
        r = queue.pop()
        if r in seen:
            continue
        p = inc / r
        if not p.exists():
            missing.add(r)
            continue
        seen.add(r)
        for m in inc_re.findall(p.read_text(encoding="utf-8", errors="replace")):
            queue.append(m if (inc / m).exists()
                         else (Path(r).parent / m).as_posix().replace("./", ""))
    sources = [inc / r for r in sorted(seen)]
    copied = set(seen)
    if missing:
        print("headers referenced but absent from the decomp (includes commented out): %s"
              % ", ".join(sorted(missing)))

    # Clear only what this script owns, so a header deleted upstream does not
    # linger. Include/Rio/ and anything else hand-written is left alone.
    if not args.dry_run:
        for entry in GENERATED_ENTRIES:
            target = OUT_ROOT / entry
            if target.is_dir():
                shutil.rmtree(target, ignore_errors=True)
            elif target.exists():
                target.unlink()

    dupes = collect_declared_functions(sources, inc)
    stats = dict(data_bound=0, data_unresolved=0, func_bound=0,
                 func_unresolved=0, macros_suppressed=0, layout_asserts=0)
    unresolved = []
    generated_text = []

    for src in sources:
        rel = src.relative_to(inc).as_posix()
        text = src.read_text(encoding="utf-8", errors="replace")
        text = rewrite_header(text, syms, stats, unresolved, rel, dupes)
        text = rewrite_includes(text, copied)
        body = PRELUDE.format(src=rel) + text
        generated_text.append(body)
        dst = OUT_ROOT / rel
        if not args.dry_run:
            dst.parent.mkdir(parents=True, exist_ok=True)
            dst.write_text(body, encoding="utf-8", newline="\n")

    print("headers written : %d -> %s" % (len(sources), OUT_ROOT))
    print("data   bound %5d   unresolved %5d" % (stats["data_bound"], stats["data_unresolved"]))
    print("funcs  bound %5d   unresolved %5d" % (stats["func_bound"], stats["func_unresolved"]))
    print("layout asserts emitted: %d (offset/size guards from the decomp's comments)" % stats["layout_asserts"])
    print("macros suppressed (CGecko owns): %d" % stats["macros_suppressed"])

    # Everything the decomp names but never declares, emitted as a raw address
    # so no symbol the decomp knows about is unreachable from a mod.
    declared = set(IDENT_RE.findall("\n".join(generated_text)))
    meta = build_symbol_meta(decomp, cvt)
    bound = emit_symbol_headers(meta, declared, args.dry_run)
    if bound:
        nested = bound.pop("_nested", 0)
        print("address bindings: %s   -> Include/Symbols/"
              % ", ".join("%s %d" % (m, n) for m, n in sorted(bound.items())))
        print("nested symbols not emitted (reached through their container): %d" % nested)

    if not args.dry_run:
        lines = ["Unresolved declarations -- no address found by name in the decomp's",
                 "config/GYQE01/**/symbols.txt. These stay commented out in the output;",
                 "add the declaration to the decomp (a typed extern or a prototype) and re-sync.", ""]
        for relpath, kind, name in sorted(unresolved):
            lines.append("%-40s %-5s %s" % (relpath, kind, name))
        REPORT.parent.mkdir(parents=True, exist_ok=True)
        REPORT.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
        print("report          : %s (%d entries)" % (REPORT, len(unresolved)))


if __name__ == "__main__":
    main()
