# ExportGameData.py / ExportMenuData.py
# Exports all user-defined data types (structs / enums / typedefs) plus all
# labeled data symbols, plain labels, and function addresses from the current
# program into ONE C header usable by CGecko-compiled C code.
#
# Output filename depends on which program is open (see PROGRAM_OUTPUT_NAMES):
#   in_game             -> GameData.h
#   AtGameSettingsScreen-> MenuData.h
#   (anything else)     -> Data.h
#
# Rerunning fully regenerates the file (atomic overwrite). This is a ONE-WAY
# street: edit names/types/sizes in Ghidra and regenerate; never hand-edit.
#
# Usage (GUI):     Script Manager -> run. Prompts for an output directory.
# Usage (headless):
#   analyzeHeadless <ProjectDir> <ProjectName> -process <ProgramName> \
#       -noanalysis -readonly -postScript ExportGameData.py <output_dir>
#
# @category ProjectRio
# @runtime Jython

import os
import re
import tempfile

from java.io import StringWriter
from java.util import ArrayList

from ghidra.program.model.data import (
    DataTypeWriter, Composite, Enum, TypeDef, FunctionDefinition,
    Array, BuiltInDataType, Union, Pointer,
)
from ghidra.program.model.symbol import SymbolType, SourceType

# ==============================================================================
# CONFIG
# ==============================================================================

# Output filename per program name (extension is stripped before matching).
PROGRAM_OUTPUT_NAMES = {
    "in_game": "GameData.h",
    "AtGameSettingsScreen": "MenuData.h",
}
DEFAULT_OUTPUT_FILENAME = "Data.h"

# Hard-coded output directory. When non-blank, the file is written here with no
# prompt. Leave as "" to fall back to the script argument (if any), then to an
# interactive folder picker.
OUTPUT_DIR = r"D:\15165\Documents\Rio Modding Project\Project Rio\ProjectRio-ASM\Game Data"

# Prepend #include "<COMMON_H_PATH>" (CGecko adds the repo root to the include
# path, so this resolves there). Common.h provides VAR_ADDRESS, byte, word, etc.
INCLUDE_COMMON_H = True
COMMON_H_PATH = "CGecko/Common.h"

# Emit our own copies of Ghidra's primitive typedefs (undefined/uchar/uint/...).
# Common.h defines byte/halfword/word/bool; these are the *other* Ghidra names.
# C11 allows identical typedef redefinition, so overlap is harmless.
EMIT_PRIMITIVE_TYPEDEFS = True

# Emit a VAR_ADDRESS fallback (matches Common.h's variadic form). Skipped when
# Common.h is included, since Common.h defines VAR_ADDRESS first.
EMIT_ADDRESS_MACROS = True

# Add __attribute__((packed)) to every struct/union so GCC's layout matches
# Ghidra's exact byte layout. Needed for sizes/offsets to line up.
PACK_STRUCTS = True

# Emit _Static_assert(sizeof(T) == N) guards. OFF by default so you always get
# a compiling header; flip True to VERIFY every struct against Ghidra's size.
# Any failure is a genuine layout disagreement (usually bitfields, or a Ghidra
# type-name conflict) to resolve in Ghidra.
EMIT_SIZE_GUARDS = False

# Ghidra's `word` is 2-byte (unsigned short); Common.h's `word` is 4-byte uint.
# Rename Ghidra's to keep its 2-byte meaning without the clash.
GHIDRA_TYPE_RENAMES = {"word": "ghidra_word"}

# Skip Ghidra auto-generated placeholder names (not real labels you made).
AUTO_NAME_PREFIXES = (
    "FUN_", "DAT_", "LAB_", "SUB_", "UNK_", "PTR_", "ARRAY_",
    "s_", "u_", "switchD_", "caseD_", "joined_r",
)

# ==============================================================================
# C-IDENTIFIER / TYPE SANITIZER  (verified standalone; comment-aware)
# ------------------------------------------------------------------------------
# Ghidra lets you name fields/enums/types with characters illegal in C, put
# spaces in names, attach trailing comments, declare pointer typedefs, and give
# enums non-int sizes. These helpers rewrite DataTypeWriter output into valid,
# size-accurate C. sanitize_ident is deterministic, so a definition and every
# reference map to the same output with no rename table.
# ==============================================================================
ILLEGAL = re.compile(r'[^A-Za-z0-9_]')

C_KEYWORDS = set("""auto break case char const continue default do double else enum
extern float for goto if inline int long register restrict return short signed
sizeof static struct switch typedef union unsigned void volatile while
_Bool _Complex _Imaginary bool true false""".split())

C_PRIMITIVES = set("unsigned signed int char short long float double void _Bool".split())

# Module-level context, set at the start of rewrite_header().
_RENAMES = {}              # e.g. {"word": "ghidra_word"} -- Ghidra builtins that clash with Common.h
_ENUM_TYPEDEF_NAMES = set()  # sanitized enum names converted to sized-int typedefs


def sanitize_ident(name, avoid_keywords=False):
    s = ILLEGAL.sub('_', name)
    if not s:
        s = '_'
    if s[0].isdigit():
        s = '_' + s
    if avoid_keywords and s in C_KEYWORDS:
        s = s + '_'
    return s


def sanitize_type_str(s):
    """Sanitize a type reference. Handles: 'pointer'->'void *', renamed builtins,
    dropping 'enum ' for enums that became sized-int typedefs, struct/union/enum
    tag sanitization, C primitives, and '*'."""
    s = s.strip()
    if not s:
        return s
    out_toks = []
    expect_tag = False
    pending_kw = None
    for tok in s.split():
        if tok in ('struct', 'union', 'enum') and not expect_tag:
            pending_kw = tok
            expect_tag = True
            continue
        lead_m = re.match(r'^\*+', tok)
        lead = lead_m.group(0) if lead_m else ''
        trail_m = re.search(r'\*+$', tok)
        trail = trail_m.group(0) if trail_m else ''
        core = tok[len(lead): len(tok) - len(trail)] if trail else tok[len(lead):]
        if core == '':
            out_toks.append(tok)  # pure stars
            expect_tag = False
            pending_kw = None
            continue
        if expect_tag:
            san = _RENAMES.get(core, sanitize_ident(core, avoid_keywords=True))
            # If this enum became a sized-int typedef, drop the 'enum' keyword
            # so the field uses the (correctly sized) typedef instead.
            if pending_kw == 'enum' and san in _ENUM_TYPEDEF_NAMES:
                out_toks.append(lead + san + trail)
            else:
                out_toks.append(pending_kw + ' ' + lead + san + trail)
            expect_tag = False
            pending_kw = None
            continue
        # bare token (no struct/union/enum keyword in front)
        if core in _RENAMES:
            out_toks.append(lead + _RENAMES[core] + trail)
        elif core == 'pointer':
            out_toks.append(lead + 'void *' + trail)
        elif core in C_PRIMITIVES:
            out_toks.append(lead + core + trail)
        else:
            out_toks.append(lead + sanitize_ident(core) + trail)
    return ' '.join(out_toks)


def _uniquify(name, seen):
    if name not in seen:
        seen.add(name)
        return name
    i = 2
    while (name + '_' + str(i)) in seen:
        i += 1
    new = name + '_' + str(i)
    seen.add(new)
    return new


def _split_field(inner):
    """Split a field declaration body (no trailing ';') into
    (type_str, stars, name_core, arrays, bitfield), tolerating spaces in the
    field name and multiword primitive types. Returns None if it can't parse
    safely (e.g. function pointers)."""
    if re.search(r'\(\s*\*', inner):
        return None  # function pointer -- leave alone
    arrays = ''
    am = re.search(r'((\s*\[[^\]]*\])+)$', inner)
    if am:
        arrays = am.group(1)
        inner = inner[:am.start()].rstrip()
    bitfield = ''
    bm = re.search(r':\s*\w+$', inner)
    if bm:
        bitfield = ' ' + inner[bm.start():]
        inner = inner[:bm.start()].rstrip()
    toks = inner.split()
    if not toks:
        return None
    if toks[0] in ('struct', 'union', 'enum'):
        if len(toks) < 3:
            return None  # no field name
        type_toks = toks[:2]
        name_toks = toks[2:]
    elif toks[0] in C_PRIMITIVES:
        i = 0
        while i < len(toks) - 1 and toks[i] in C_PRIMITIVES:
            i += 1
        if i == 0:
            return None  # lone primitive, no field name
        type_toks = toks[:i]
        name_toks = toks[i:]
    else:
        if len(toks) < 2:
            return None
        type_toks = toks[:1]
        name_toks = toks[1:]
    name_str = ' '.join(name_toks)
    m = re.match(r'^(\**)\s*(.*)$', name_str)
    stars = m.group(1)
    core = m.group(2).strip()
    if not core:
        return None
    return (' '.join(type_toks), stars, core, arrays, bitfield)


def _san_fnptr_name(declarator, seen_fields):
    """Sanitize the field name inside a function-pointer declarator, e.g.
    '(*functionPtr2(sounds))(void *)' -> '(*functionPtr2_sounds_)(void *)',
    preserving any [N] array suffix. Returns the declarator unchanged if it
    isn't the ordinary (*name)(...) shape."""
    m = re.match(
        r'(\(\s*\*\s*)([A-Za-z_]\w*(?:\([^)]*\))?)((?:\s*\[[^\]]*\])*)(\s*\).*)$',
        declarator)
    if not m:
        return declarator
    pre, nm, arr, rest = m.groups()
    san = _uniquify(sanitize_ident(nm, avoid_keywords=True), seen_fields)
    return pre + san + arr + rest


def _process_field_line(indent, body, seen_fields):
    inner = body[:-1].rstrip()
    parsed = _split_field(inner)
    if parsed is None:
        # _split_field bails on complex declarators (function pointers,
        # pointer-to-array). Sanitize the leading TYPE specifier (so Ghidra
        # names like 'pointer' don't leak through) and, for the (*name)(...)
        # function-pointer shape, the embedded field name too -- otherwise a
        # Ghidra annotation like 'functionPtr2(sounds)' lands inside the
        # declarator and C reads the field as a function.
        if re.search(r'\(\s*\*', inner):
            paren = inner.find('(')
            type_part = inner[:paren].rstrip()
            declarator = _san_fnptr_name(inner[paren:], seen_fields)
            if type_part:
                return indent + sanitize_type_str(type_part) + ' ' + declarator + ';'
            return indent + declarator + ';'
        return indent + body
    type_str, stars, name_core, arrays, bitfield = parsed
    san_name = _uniquify(sanitize_ident(name_core, avoid_keywords=True), seen_fields)
    typ2 = sanitize_type_str(type_str)
    return indent + typ2 + ' ' + stars + san_name + arrays + bitfield + ';'


def _split_enum_const(body):
    """Return (name, rest) for an enum constant line body, tolerating spaces in
    the name. rest keeps '=value' and any trailing comma/comment verbatim."""
    idx = body.rfind('=')
    if idx >= 0:
        name = body[:idx].strip()
        rest = body[idx:]
    else:
        m = re.match(r'^(.*?)(\s*,?\s*)$', body)
        name = m.group(1).strip()
        rest = m.group(2)
    return name, rest


def _process_enum_const(indent, body, enum_name, prefix_set, seen_consts):
    name, rest = _split_enum_const(body)
    if not name:
        return indent + body
    san = sanitize_ident(name, avoid_keywords=True)
    if enum_name is not None:
        san = sanitize_ident(enum_name) + '_' + san
    san = _uniquify(san, seen_consts)
    return indent + san + rest


def _sized_int(length, signed):
    table = {
        (1, False): 'unsigned char', (1, True): 'signed char',
        (2, False): 'unsigned short', (2, True): 'short',
        (4, False): 'unsigned int', (4, True): 'int',
        (8, False): 'unsigned long long', (8, True): 'long long',
    }
    return table.get((length, bool(signed)), 'int')


# --- header line detectors -------------------------------------------------
_RE_FWD = re.compile(r'^typedef\s+(struct|union)\s+(\S.*);$')
_RE_STRUCT_OPEN = re.compile(r'^(?:typedef\s+)?(struct|union)\s+(\S+)\s*\{$')
_RE_STRUCT_OPEN_ANON = re.compile(r'^(?:typedef\s+)?(struct|union)\s*\{$')
_RE_ENUM_OPEN = re.compile(r'^(?:typedef\s+)?enum\s+(\S+)\s*\{$')
_RE_ENUM_OPEN_ANON = re.compile(r'^(?:typedef\s+)?enum\s*\{$')
_RE_PLAIN_TYPEDEF = re.compile(r'^typedef\s+(.*\S)\s+(\**\w[\w ]*?)(\s*\[[^\]]*\])*\s*;$')


def _enum_start(s):
    return (_RE_ENUM_OPEN.match(s) is not None) or (_RE_ENUM_OPEN_ANON.match(s) is not None)


def _plain_typedef_name(s):
    """Best-effort extract the declared name of a plain (non-aggregate) typedef."""
    inner = s[len('typedef'):].strip()
    if inner.endswith(';'):
        inner = inner[:-1].rstrip()
    inner = re.sub(r'(\s*\[[^\]]*\])+$', '', inner).rstrip()
    if re.search(r'\(\s*\*', inner):
        return None  # function pointer typedef
    toks = inner.split()
    if len(toks) < 2:
        return None
    last = toks[-1]
    last = last.lstrip('*')
    if not last:
        return None
    return last



def _split_code_comment(s):
    """Split off a trailing (or full-line) C comment so structural parsing and
    brace counting ignore it. Returns (code, comment)."""
    m = re.search(r'/\*|//', s)
    if m:
        return s[:m.start()].rstrip(), s[m.start():]
    return s, ''


def scan_header_names(text):
    """First pass: collect (prefix_set, type_names, const_names).
    prefix_set = enum constants that must be prefixed (collide across enums or
    with a type name). type_names/const_names are returned for seeding the macro
    NameRegistry so #defines never collide with a typedef or enum constant."""
    lines = text.split('\n')
    freq = {}
    type_names = set()
    const_names = set()
    in_enum = False
    seen_enumA = set()
    skip = False
    anon = [0]

    for raw in lines:
        s = raw.strip()
        if not in_enum:
            mo = _RE_ENUM_OPEN.match(s)
            mo_anon = _RE_ENUM_OPEN_ANON.match(s)
            if mo or mo_anon:
                in_enum = True
                if mo:
                    key = sanitize_ident(mo.group(1))
                    type_names.add(key)  # enum becomes a typedef -> ordinary id
                else:
                    anon[0] += 1
                    key = '__anon%d__' % anon[0]
                skip = key in seen_enumA
                seen_enumA.add(key)
                continue
            mf = _RE_FWD.match(s)
            if mf:
                rest = mf.group(1) and mf.group(2)
                for part in mf.group(2).split(','):
                    for t in part.split():
                        type_names.add(sanitize_ident(t.lstrip('*')))
                continue
            ms = _RE_STRUCT_OPEN.match(s)
            if ms:
                type_names.add(sanitize_ident(ms.group(2)))
                continue
            if s.startswith('typedef '):
                nm = _plain_typedef_name(s)
                if nm:
                    type_names.add(sanitize_ident(nm))
                continue
        else:
            if s.startswith('}'):
                in_enum = False
            elif s and not s.startswith('/*') and not s.startswith('//') and not s.startswith('*') and not skip:
                nm, _ = _split_enum_const(s)
                if nm:
                    cn = sanitize_ident(nm, avoid_keywords=True)
                    const_names.add(cn)
                    freq[cn] = freq.get(cn, 0) + 1

    colliding = set(n for n, c in freq.items() if c > 1)
    prefix_set = colliding | (const_names & type_names)
    return prefix_set, type_names, const_names


def rewrite_header(text, prefix_set, enum_meta, renames, pack_structs=True, stub_names=None):
    """Second pass. enum_meta = {sanitized_enum_name: (length, signed)}.
    renames = {ghidra_name: replacement}. Comment-aware: trailing /*...*/ and
    // comments are preserved but never confuse field/brace parsing."""
    global _RENAMES, _ENUM_TYPEDEF_NAMES
    _RENAMES = dict(renames or {})
    _ENUM_TYPEDEF_NAMES = set(enum_meta.keys())

    lines = text.split('\n')
    out = []
    seen_fwd = set()
    seen_struct = set(stub_names or ())  # already defined by zero-length stubs
    seen_enum = set()
    state = 'top'
    depth = 0
    cur_enum_name = None
    cur_enum_san = None
    seen_fields = set()
    seen_consts = set()
    dropped = {'struct': 0, 'enum': 0, 'fwd': 0}

    for raw in lines:
        s = raw.strip()
        indent = raw[:len(raw) - len(raw.lstrip())]
        code, comment = _split_code_comment(s)
        code = code.rstrip()
        csp = ('  ' + comment) if comment else ''

        # comment-only or blank line: emit verbatim, no state/brace change
        if not code:
            out.append(raw)
            continue

        if state == 'top':
            ms = _RE_STRUCT_OPEN.match(code)
            ms_anon = _RE_STRUCT_OPEN_ANON.match(code)
            me = _RE_ENUM_OPEN.match(code)
            me_anon = _RE_ENUM_OPEN_ANON.match(code)
            mf = _RE_FWD.match(code)

            if ms or ms_anon:
                kw = (ms.group(1) if ms else ms_anon.group(1))
                if ms:
                    name = sanitize_ident(ms.group(2), avoid_keywords=True)
                    if name in seen_struct:
                        state = 'skip_struct'
                        depth = code.count('{') - code.count('}')
                        dropped['struct'] += 1
                        continue
                    seen_struct.add(name)
                    prefix = 'typedef ' if code.startswith('typedef') else ''
                    out.append(indent + prefix + kw + ' ' + name + ' {' + csp)
                else:
                    prefix = 'typedef ' if code.startswith('typedef') else ''
                    out.append(indent + prefix + kw + ' {' + csp)
                seen_fields = set()
                state = 'struct'
                depth = code.count('{') - code.count('}')
            elif me or me_anon:
                if me:
                    name = me.group(1)
                    san = sanitize_ident(name, avoid_keywords=True)
                    if san in seen_enum:
                        state = 'skip_enum'
                        depth = code.count('{') - code.count('}')
                        dropped['enum'] += 1
                        continue
                    seen_enum.add(san)
                else:
                    name = None
                    san = None
                cur_enum_name = name
                cur_enum_san = san
                seen_consts = set()
                converted = (san is not None and san in enum_meta)
                if converted:
                    out.append(indent + 'enum {' + csp)
                else:
                    prefix = 'typedef ' if code.startswith('typedef') else ''
                    out.append(indent + prefix + 'enum ' + (san + ' ' if san else '') + '{' + csp)
                state = 'enum'
                depth = code.count('{') - code.count('}')
            elif mf:
                kw = mf.group(1)
                rest = mf.group(2)
                new_parts = []
                for part in rest.split(','):
                    toks = part.split()
                    new_toks = []
                    for t in toks:
                        lm = re.match(r'^\*+', t)
                        ld = lm.group(0) if lm else ''
                        new_toks.append(ld + sanitize_ident(t[len(ld):], avoid_keywords=True))
                    new_parts.append(' '.join(new_toks))
                fwd_key = ', '.join(new_parts)
                if fwd_key in seen_fwd:
                    dropped['fwd'] += 1
                    continue
                seen_fwd.add(fwd_key)
                out.append(indent + 'typedef ' + kw + ' ' + ', '.join(new_parts) + ';' + csp)
            elif code.startswith('typedef ') and '{' not in code and _plain_typedef_name(code):
                inner = code[len('typedef'):].strip()
                if inner.endswith(';'):
                    inner = inner[:-1].rstrip()
                arr = ''
                am = re.search(r'((\s*\[[^\]]*\])+)$', inner)
                if am:
                    arr = am.group(1)
                    inner = inner[:am.start()].rstrip()
                toks = inner.split()
                nm = toks[-1].lstrip('*')
                stars = toks[-1][:len(toks[-1]) - len(toks[-1].lstrip('*'))]
                type_part = ' '.join(toks[:-1])
                nm2 = _RENAMES.get(nm, sanitize_ident(nm, avoid_keywords=True))
                out.append(indent + 'typedef ' + sanitize_type_str(type_part) + ' ' + stars + nm2 + arr + ';' + csp)
            else:
                out.append(raw)

        elif state == 'struct':
            depth += code.count('{') - code.count('}')
            if code.startswith('}') and depth <= 0:
                after = code[1:]
                if pack_structs:
                    out.append(indent + '} __attribute__((packed))' + after + csp)
                else:
                    out.append(indent + code + csp)
                state = 'top'
            elif code.endswith(';') and not code.startswith('}') and '{' not in code:
                out.append(_process_field_line(indent, code, seen_fields) + csp)
            else:
                out.append(raw)

        elif state == 'skip_struct':
            depth += code.count('{') - code.count('}')
            if depth <= 0 and code.startswith('}'):
                state = 'top'
            continue

        elif state == 'enum':
            if code.startswith('}'):
                converted = (cur_enum_san is not None and cur_enum_san in enum_meta)
                out.append(indent + '};' + csp)
                if converted:
                    length, signed = enum_meta[cur_enum_san]
                    out.append(indent + 'typedef ' + _sized_int(length, signed)
                               + ' ' + cur_enum_san + ';')
                state = 'top'
                cur_enum_name = None
                cur_enum_san = None
            else:
                out.append(_process_enum_const(indent, code, cur_enum_name, prefix_set, seen_consts) + csp)

        elif state == 'skip_enum':
            if code.startswith('}'):
                state = 'top'
            continue

    return '\n'.join(out), dropped

# ==============================================================================
# SANITIZER CONTEXT  (set before generating macros OR rewriting types so type
# references resolve identically in both)
# ==============================================================================

def set_sanitize_context(enum_meta, renames):
    global _RENAMES, _ENUM_TYPEDEF_NAMES
    _RENAMES = dict(renames or {})
    _ENUM_TYPEDEF_NAMES = set(enum_meta.keys())


# ==============================================================================
# HEADER BOILERPLATE
# ==============================================================================

GHIDRA_WORD_BLOCK = """\
// ghidra_word: Ghidra's 2-byte `word` (Common.h's `word` is 4-byte uint).
#ifndef GHIDRA_WORD_DEFINED
#define GHIDRA_WORD_DEFINED
typedef unsigned short ghidra_word;
#endif
"""

PRIMITIVE_BLOCK = """\
// --- Ghidra primitive types (Common.h covers byte/halfword/word/bool) -------
#ifndef GHIDRA_PRIMITIVE_TYPES
#define GHIDRA_PRIMITIVE_TYPES
typedef unsigned char       undefined;
typedef unsigned char       undefined1;
typedef unsigned short      undefined2;
typedef unsigned int        undefined4;
typedef unsigned long long  undefined8;
typedef unsigned char       uchar;
typedef unsigned short      ushort;
typedef unsigned int        uint;
typedef unsigned long       ulong;
typedef long long           longlong;
typedef unsigned long long  ulonglong;
#endif // GHIDRA_PRIMITIVE_TYPES
"""

# Matches Common.h's macros exactly (used only if Common.h isn't included --
# otherwise Common.h defines these first and this block is skipped).
ADDRESS_MACRO_BLOCK = """\
// --- address-macro fallback (matches Common.h; skipped if Common.h is included)
#ifndef VAR_ADDRESS
#define VAR_ADDRESS(type, addr) (*(type *)(addr))
#define ARRAY_1D_ADDRESS(type, n, addr) (*(type (*)[n])(addr))
#define ARRAY_2D_ADDRESS(type, r, c, addr) (*(type (*)[r][c])(addr))
#define ARRAY_3D_ADDRESS(type, x, y, z, addr) (*(type (*)[x][y][z])(addr))
#define ARRAY_4D_ADDRESS(type, a, b, c, d, addr) (*(type (*)[a][b][c][d])(addr))
#define ARRAY_5D_ADDRESS(type, a, b, c, d, e, addr) (*(type (*)[a][b][c][d][e])(addr))
#endif
"""


def build_prelude(program_name):
    head = (
        "// =============================================================================\n"
        "//  AUTO-GENERATED by ExportGameData.py. DO NOT HAND-EDIT.\n"
        "//  Regenerate from Ghidra after relabeling. Program: %s\n"
        "// =============================================================================\n"
        "#pragma once\n" % program_name
    )
    if INCLUDE_COMMON_H:
        head += '#include "%s"\n' % COMMON_H_PATH
    head += "\n" + GHIDRA_WORD_BLOCK + "\n"
    if EMIT_PRIMITIVE_TYPEDEFS:
        head += PRIMITIVE_BLOCK + "\n"
    if EMIT_ADDRESS_MACROS:
        head += ADDRESS_MACRO_BLOCK
    return head


# ==============================================================================
# HELPERS
# ==============================================================================

def resolve_output_filename(program_name):
    base = program_name
    for ext in (".dol", ".elf", ".bin", ".rel", ".map"):
        if base.lower().endswith(ext):
            base = base[:-len(ext)]
            break
    return PROGRAM_OUTPUT_NAMES.get(base, DEFAULT_OUTPUT_FILENAME)


def is_exportable_name(name):
    if not name:
        return False
    for p in AUTO_NAME_PREFIXES:
        if name.startswith(p):
            return False
    return True


def addr_hex(addr):
    return "0x" + str(addr).upper().lstrip("0X").rjust(8, "0")


class NameRegistry(object):
    """Avoid duplicate #define names across macro sections, and avoid colliding
    with any typedef name or enum constant (seeded up front)."""
    def __init__(self):
        self.seen = set()

    def seed(self, names):
        for n in names:
            self.seen.add(n)

    def claim(self, name, addr):
        if name not in self.seen:
            self.seen.add(name)
            return name
        alt = "%s_%s" % (name, str(addr))
        while alt in self.seen:
            alt = alt + "_"
        self.seen.add(alt)
        return alt


# ==============================================================================
# DATA-TYPE EXPORT
# ==============================================================================

def collect_enum_meta(dtm):
    """Map each enum's sanitized name -> (byte_length, is_signed). Used to make
    enum-typed struct fields exactly the size Ghidra recorded (C enums are
    int-sized by default, which would corrupt offsets and sizes)."""
    meta = {}
    it = dtm.getAllDataTypes()
    while it.hasNext():
        dt = it.next()
        if isinstance(dt, Enum):
            try:
                signed = False
                for v in dt.getValues():
                    if v < 0:
                        signed = True
                        break
                meta[sanitize_ident(dt.getDisplayName(), avoid_keywords=True)] = (dt.getLength(), signed)
            except Exception:
                pass
    return meta


def build_types_raw(dtm):
    """Raw DataTypeWriter output (dependency-ordered, same engine as the GUI
    'Export C Header'). Sanitization happens afterward."""
    wanted = ArrayList()
    it = dtm.getAllDataTypes()
    while it.hasNext():
        dt = it.next()
        if isinstance(dt, BuiltInDataType):
            continue
        if isinstance(dt, (Composite, Enum, TypeDef, FunctionDefinition)):
            wanted.add(dt)
    sw = StringWriter()
    writer = DataTypeWriter(dtm, sw)
    writer.write(wanted, monitor)
    return sw.toString()


def build_zero_length_stubs(dtm):
    """Ghidra zero-length composites become forward-only (incomplete) in C, so
    they can't be used as array elements or by value. Emit a 1-byte stub for
    each, BEFORE the main types, so those uses compile. (These are placeholders;
    give them a real layout in Ghidra to fix the size.)"""
    lines = []
    seen = set()
    it = dtm.getAllDataTypes()
    while it.hasNext():
        dt = it.next()
        if isinstance(dt, Composite) and dt.isZeroLength():
            nm = sanitize_ident(dt.getDisplayName())
            if nm in seen:
                continue
            seen.add(nm)
            kw = "union" if isinstance(dt, Union) else "struct"
            lines.append("%s %s { unsigned char _opaque_stub; } __attribute__((packed));"
                         % (kw, nm))
    if lines:
        lines.insert(0, "// --- Stubs for zero-length Ghidra types (give them a real layout to fix) ---")
    return lines, seen


def build_size_guards(dtm):
    """_Static_assert size guards, sanitized + deduped. Skips zero-length
    (stubbed) types. Names match what the rewriter emits."""
    lines = ["// --- Struct size guards (set EMIT_SIZE_GUARDS=True to verify) ---"]
    seen = set()
    it = dtm.getAllDataTypes()
    while it.hasNext():
        dt = it.next()
        if isinstance(dt, Composite) and not dt.isZeroLength():
            nm = sanitize_ident(dt.getDisplayName())
            if nm in seen:
                continue
            seen.add(nm)
            lines.append(
                '_Static_assert(sizeof(%s) == 0x%X, '
                '"Size mismatch vs Ghidra: %s");' % (nm, dt.getLength(), nm))
    return lines


# ==============================================================================
# MACRO SECTIONS
# ==============================================================================

def build_data_symbols_section(names):
    """Labeled, defined data -> address macros matching Common.h:
    scalars use VAR_ADDRESS(type, addr); N-dimensional arrays use the named
    per-arity macros ARRAY_1D_ADDRESS(type, n, addr) through
    ARRAY_5D_ADDRESS(type, a, b, c, d, e, addr), always against the base
    element type. 6D+ flattens to a 1D total with a warning."""
    lines = []
    listing = currentProgram.getListing()
    items = []
    di = listing.getDefinedData(True)
    while di.hasNext():
        data = di.next()
        sym = data.getPrimarySymbol()
        if sym is None or not is_exportable_name(sym.getName()):
            continue
        items.append((data.getMinAddress(), sym.getName(), data.getDataType()))
    items.sort(key=lambda t: t[0])
    for addr, name, dt in items:
        name = names.claim(sanitize_ident(name), addr)
        if isinstance(dt, Array):
            # Peel every dimension so multi-dimensional arrays resolve to the
            # *base* element type plus each count, e.g. StatisticsBatter[2][9]
            # -> ARRAY_2D_ADDRESS(StatisticsBatter, 2, 9, addr). Reading
            # dt.getDataType() once (the old behavior) named the inner array
            # "StatisticsBatter_9_", a type that is referenced but never declared.
            dims = []
            cur = dt
            while isinstance(cur, Array):
                dims.append(cur.getNumElements())
                cur = cur.getDataType()
            elem = sanitize_type_str(cur.getDisplayName())
            if len(dims) > 5:
                # Common.h tops out at ARRAY_5D_ADDRESS; flatten anything deeper
                # to the total element count so the macro still names a real type.
                total = 1
                for d in dims:
                    total *= d
                print("[WARN] %s: %dD array flattened to [%d]; Common.h supports up to 5D"
                      % (name, len(dims), total))
                dims = [total]
            dims_str = ", ".join(str(d) for d in dims)
            lines.append("#define %s ARRAY_%dD_ADDRESS(%s, %s, %s)"
                         % (name, len(dims), elem, dims_str, addr_hex(addr)))
        else:
            elem = sanitize_type_str(dt.getDisplayName())
            lines.append("#define %s VAR_ADDRESS(%s, %s)"
                         % (name, elem, addr_hex(addr)))
    return "\n".join(lines)


def build_plain_labels_section(names):
    """Labels with no defined data -> bare address macros."""
    lines = []
    st = currentProgram.getSymbolTable()
    listing = currentProgram.getListing()
    items = []
    it = st.getSymbolIterator()
    while it.hasNext():
        sym = it.next()
        if sym.getSymbolType() != SymbolType.LABEL or not sym.isPrimary():
            continue
        addr = sym.getAddress()
        if not addr.isMemoryAddress():
            continue
        if listing.getDefinedDataAt(addr) is not None:
            continue
        if not is_exportable_name(sym.getName()):
            continue
        items.append((addr, sym.getName()))
    items.sort(key=lambda t: t[0])
    for addr, name in items:
        name = names.claim(sanitize_ident(name) + "_ADDR", addr)
        lines.append("#define %s %s" % (name, addr_hex(addr)))
    return "\n".join(lines)


def _param_c_parts(dt, pname):
    """Render one function parameter as C. Returns (sig_decl, cast_type):
    sig_decl goes in the wrapper's signature, cast_type in the function-pointer
    cast. Handles the declarator shapes getDisplayName() gets wrong:
      byte[12][2]      -> 'byte pname[12][2]'      / 'byte [12][2]'   (decays)
      byte[2] *        -> 'byte (*pname)[2]'       / 'byte (*)[2]'
      func / func *    -> 'ret (*pname)(args)'     / 'ret (*)(args)'  (adjusts)
    Everything else falls through to the sanitized display name."""
    # Peel pointer levels to see what is ultimately pointed at.
    stars = ""
    cur = dt
    while isinstance(cur, Pointer):
        stars += "*"
        cur = cur.getDataType()
        if cur is None:               # generic 'pointer' with no pointee type
            return ("void *%s %s" % (stars[1:], pname)).replace("  ", " "), \
                   "void *" + stars[1:]
    if isinstance(cur, TypeDef):
        base_dt = cur.getBaseDataType()
        if isinstance(base_dt, (FunctionDefinition, Array)):
            cur = base_dt             # look through typedefs of the tricky kinds
    if isinstance(cur, FunctionDefinition):
        ret = sanitize_type_str(cur.getReturnType().getDisplayName())
        if ret == "undefined":
            ret = "void"
        args = [sanitize_type_str(a.getDataType().getDisplayName())
                for a in cur.getArguments()]
        if cur.hasVarArgs():
            args.append("...")
        astr = ", ".join(args) if args else "void"
        ptr = stars if stars else "*"     # bare function type adjusts to pointer
        return "%s (%s%s)(%s)" % (ret, ptr, pname, astr), \
               "%s (%s)(%s)" % (ret, ptr, astr)
    if isinstance(cur, Array):
        dims = []
        inner = cur
        while isinstance(inner, Array):
            dims.append(inner.getNumElements())
            inner = inner.getDataType()
        base = sanitize_type_str(inner.getDisplayName())
        dim_str = "".join("[%d]" % d for d in dims)
        if stars:                         # pointer-to-array: byte (*x)[2]
            return "%s (%s%s)%s" % (base, stars, pname, dim_str), \
                   "%s (%s)%s" % (base, stars, dim_str)
        return "%s %s%s" % (base, pname, dim_str), \
               "%s %s" % (base, dim_str)
    t = sanitize_type_str(dt.getDisplayName())
    return "%s %s" % (t, pname), t


def build_functions_section(names):
    """Named functions -> static inline wrappers that call the real address
    through a typed function pointer, e.g.
        static inline void PlaySound(int soundID, int volume_, int pan, int flags) {
            ((void(*)(int, int, int, int))0x800C836C)(soundID, volume_, pan, flags);
        }
    Each parameter name is de-conflicted against the macro namespace (data
    symbols, labels, other functions, type/enum names already in the registry)
    so a parameter sharing a name with a #define can't be macro-expanded inside
    the signature. True varargs functions can't forward '...' from a wrapper
    body, so they fall back to the FUNCTION_ADDRESS macro."""
    lines = []
    fm = currentProgram.getFunctionManager()
    items = []
    for fn in fm.getFunctions(True):
        if fn.isThunk() or fn.isExternal():
            continue
        if not is_exportable_name(fn.getName()):
            continue
        items.append(fn)
    items.sort(key=lambda f: f.getEntryPoint())
    # First pass: claim every function name up front so parameter de-confliction
    # below sees the complete namespace (every sibling function, not just the
    # ones at lower addresses).
    claimed = []
    for fn in items:
        nm = names.claim(sanitize_ident(fn.getName()), fn.getEntryPoint())
        claimed.append((fn, nm))
    for fn, name in claimed:
        addr = fn.getEntryPoint()
        sig = fn.getSignature()
        ret = sanitize_type_str(sig.getReturnType().getDisplayName())
        if ret == "undefined":      # Ghidra's unset/unknown return -> treat as void
            ret = "void"
        # De-conflict each parameter name against the macro namespace and against
        # sibling parameters; suffix '_' until it is unique. Then render each
        # parameter's signature declarator and cast type together so array and
        # function-pointer parameters come out as legal C (see _param_c_parts).
        pnames = []
        used = set()
        for p in fn.getParameters():
            pn = sanitize_ident(p.getName(), avoid_keywords=True)
            while pn in names.seen or pn in used:
                pn = pn + "_"
            used.add(pn)
            pnames.append(pn)
        sig_decls = []
        cast_list = []
        for p, pn in zip(fn.getParameters(), pnames):
            sd, ct = _param_c_parts(p.getDataType(), pn)
            sig_decls.append(sd)
            cast_list.append(ct)
        # Varargs: a wrapper body cannot forward '...', so emit the macro form.
        if sig.hasVarArgs():
            lines.append("#define %s FUNCTION_ADDRESS(%s, %s, %s)"
                         % (name, ret, addr_hex(addr), ", ".join(cast_list + ["..."])))
            continue
        cast_types = ", ".join(cast_list) if cast_list else "void"
        if sig_decls:
            sig_str = ", ".join(sig_decls)
            call = ", ".join(pnames)
        else:
            sig_str = "void"
            call = ""
        inner = "((%s(*)(%s))%s)(%s);" % (ret, cast_types, addr_hex(addr), call)
        body = inner if ret == "void" else ("return " + inner)
        lines.append("static inline %s %s(%s) { %s }"
                     % (ret, name, sig_str, body))
    return "\n".join(lines)


# ==============================================================================
# MAIN
# ==============================================================================

def main():
    program_name = currentProgram.getDomainFile().getName()
    out_name = resolve_output_filename(program_name)

    out_dir = OUTPUT_DIR
    if not out_dir:
        args = getScriptArgs()
        if len(args) > 0:
            out_dir = args[0]
        else:
            out_dir = askDirectory("Output directory for %s" % out_name,
                                   "Select").getAbsolutePath()
    if not os.path.isdir(out_dir):
        raise IOError("Output directory does not exist: %s" % out_dir)

    dtm = currentProgram.getDataTypeManager()

    println("[INFO] Program '%s' -> %s" % (program_name, out_name))
    println("[INFO] Collecting enum sizes...")
    enum_meta = collect_enum_meta(dtm)
    set_sanitize_context(enum_meta, GHIDRA_TYPE_RENAMES)

    println("[INFO] Exporting + sanitizing data types...")
    raw_types = build_types_raw(dtm)
    prefix_set, type_names, const_names = scan_header_names(raw_types)

    names = NameRegistry()
    names.seed(type_names)
    names.seed(const_names)

    println("[INFO] Exporting data symbols...")
    data_text = build_data_symbols_section(names)
    println("[INFO] Exporting plain labels...")
    labels_text = build_plain_labels_section(names)
    println("[INFO] Exporting functions...")
    funcs_text = build_functions_section(names)

    stub_lines, stub_names = build_zero_length_stubs(dtm)

    clean_types, dropped = rewrite_header(
        raw_types, prefix_set, enum_meta, GHIDRA_TYPE_RENAMES, PACK_STRUCTS,
        stub_names=stub_names)

    types_block = ""
    if stub_lines:
        types_block += "\n".join(stub_lines) + "\n\n"
    types_block += clean_types
    if EMIT_SIZE_GUARDS:
        types_block += "\n" + "\n".join(build_size_guards(dtm)) + "\n"

    parts = [
        build_prelude(program_name),
        "// =============================================================================",
        "//  DATA TYPES (structs / enums / typedefs)",
        "// =============================================================================",
        types_block,
        "// =============================================================================",
        "//  DATA SYMBOLS (labeled globals)",
        "// =============================================================================",
        data_text,
        "// =============================================================================",
        "//  PLAIN LABELS (no defined data; raw addresses)",
        "// =============================================================================",
        labels_text,
        "// =============================================================================",
        "//  FUNCTIONS (typed function-pointer macros -- call directly from C)",
        "// =============================================================================",
        funcs_text,
        "",
    ]
    output = "\n\n".join(parts)

    final_path = os.path.join(out_dir, out_name)
    fd, tmp_path = tempfile.mkstemp(suffix=".h.tmp", dir=out_dir)
    try:
        f = os.fdopen(fd, "w")
        f.write(output)
        f.close()
        if os.path.exists(final_path):
            os.remove(final_path)
        os.rename(tmp_path, final_path)
    finally:
        if os.path.exists(tmp_path):
            os.remove(tmp_path)

    println("[INFO] Wrote %s" % final_path)
    println("[INFO]   data symbols      : %d" % len(data_text.splitlines()))
    println("[INFO]   plain labels      : %d" % len(labels_text.splitlines()))
    println("[INFO]   functions         : %d" % len(funcs_text.splitlines()))
    println("[INFO]   enums sized       : %d" % len(enum_meta))
    println("[INFO]   zero-length stubs : %d" % (len(stub_lines) - 1 if stub_lines else 0))
    println("[INFO]   enum-const clashes prefixed : %d" % len(prefix_set))
    println("[INFO]   duplicate defs dropped      : %d struct(s), %d enum(s), %d fwd"
            % (dropped['struct'], dropped['enum'], dropped['fwd']))
    if not EMIT_SIZE_GUARDS:
        println("[INFO] Size guards OFF. Set EMIT_SIZE_GUARDS=True to verify sizes vs Ghidra.")


main()