# ExportGameData.py / ExportMenuData.py
# Exports all user-defined data types (structs / enums / typedefs) plus all
# labeled data symbols, plain labels, and function addresses from the current
# program into ONE C header usable by CGecko-compiled C code.
#
# Output filename depends on which program is open (see PROGRAM_OUTPUT_NAMES):
#   in_game             -> MatchData.h
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
    "in_game": "MatchData.h",
    "AtGameSettingsScreen": "MenuData.h",
}
DEFAULT_OUTPUT_FILENAME = "Data.h"

# --- Shared-DOL split -------------------------------------------------------
# MSSB loads RELs at runtime above the DOL. Symbols BELOW SHARED_CUTOFF live in
# the DOL (shared, constant at runtime) and are factored into GLOBAL_HEADER;
# symbols AT/ABOVE it are REL-specific and go to the per-program context header
# (MatchData.h / MenuData.h). Each context header #includes GLOBAL_HEADER.
SHARED_CUTOFF = 0x803CD310
GLOBAL_HEADER = "GlobalData.h"

# The shared (below-cutoff) region is MERGED from both programs: an address
# labeled in only one program is exported from that program; when both label
# the same address, the richer entry wins (function > data > label; then
# user-defined name/signature source; then parameter count), and a genuine tie
# goes to the canonical program. GlobalData.h also carries the other program's
# "top-up" types (types canonical lacks) so merged symbols can reference them.
# Run the export from the canonical program; it opens the other via a picker.
CANONICAL_PROGRAM = "in_game"
# Program name of the other REL context (used to label the picker + reports).
OTHER_PROGRAM_NAME = "AtGameSettingsScreen"
# Reconciliation report for the shared region (written next to the headers).
DIFF_REPORT_NAME = "SharedRegion_report.txt"
# Set False to export just the current program the old way (single header, no
# split) -- useful for debugging one program in isolation.
ENABLE_SHARED_SPLIT = True


# Hard-coded output directory. When non-blank, the file is written here with no
# prompt. Leave as "" to fall back to the script argument (if any), then to an
# interactive folder picker.
OUTPUT_DIR = r"E:\Project Rio\ProjectRio-ASM\Include"

# Subfolder (under the output directory) each header is written into. #include
# lines reference them as "Include/<subfolder>/<name>" (CGecko adds the repo
# root to the include path). Files with no entry here (e.g. the diff report,
# the Data.h fallback) land directly in the output directory.
HEADER_SUBDIRS = {
    GLOBAL_HEADER: "Global",
    "MatchData.h": "Match",
    "MenuData.h": "Menu",
}

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

# Names already taken by Common.h / the primitive block. A #define with one of
# these names would poison every later use of that identifier, so the macro
# registry is seeded with them (a colliding symbol gets an address suffix).
RESERVED_IDENTIFIERS = (
    "bool", "false", "true", "byte", "halfword", "word", "ghidra_word",
    "VAR_ADDRESS", "ARRAY_1D_ADDRESS", "ARRAY_2D_ADDRESS", "ARRAY_3D_ADDRESS",
    "ARRAY_4D_ADDRESS", "ARRAY_5D_ADDRESS", "FUNCTION_ADDRESS",
    "READ_GAME_REG", "WRITE_GAME_REG", "READ_REG",
    "LEN", "SQUARE", "OFFSET_OF", "LOAD_FLOAT", "FP", "COMMON_H",
    "undefined", "undefined1", "undefined2", "undefined4", "undefined8",
    "uchar", "ushort", "uint", "ulong", "longlong", "ulonglong",
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


def _san_fnptr_name(declarator, seen_fields, name_sink=None):
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
    if name_sink is not None:
        name_sink.add(san)
    return pre + san + arr + rest


def _process_field_line(indent, body, seen_fields, name_sink=None):
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
            declarator = _san_fnptr_name(inner[paren:], seen_fields, name_sink)
            if type_part:
                return indent + sanitize_type_str(type_part) + ' ' + declarator + ';'
            return indent + declarator + ';'
        return indent + body
    type_str, stars, name_core, arrays, bitfield = parsed
    san_name = _uniquify(sanitize_ident(name_core, avoid_keywords=True), seen_fields)
    if name_sink is not None:
        name_sink.add(san_name)
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


def _process_enum_const(indent, body, enum_name, prefix_set, seen_consts, name_sink=None):
    name, rest = _split_enum_const(body)
    if not name:
        return indent + body
    san = sanitize_ident(name, avoid_keywords=True)
    if enum_name is not None:
        san = sanitize_ident(enum_name) + '_' + san
    san = _uniquify(san, seen_consts)
    if name_sink is not None:
        name_sink.add(san)
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


def rewrite_header(text, prefix_set, enum_meta, renames, pack_structs=True, stub_names=None,
                   name_sink=None):
    """Second pass. enum_meta = {sanitized_enum_name: (length, signed)}.
    renames = {ghidra_name: replacement}. Comment-aware: trailing /*...*/ and
    // comments are preserved but never confuse field/brace parsing.
    name_sink (a set, optional) collects every emitted struct-field and enum-
    constant name so #define macros can be kept disjoint from all of them."""
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
                out.append(_process_field_line(indent, code, seen_fields, name_sink) + csp)
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
                out.append(_process_enum_const(indent, code, cur_enum_name, prefix_set,
                                               seen_consts, name_sink) + csp)

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


def build_prelude(program_name, include_global=False):
    head = (
        "// =============================================================================\n"
        "//  AUTO-GENERATED by ExportGameData.py. DO NOT HAND-EDIT.\n"
        "//  Regenerate from Ghidra after relabeling. Program: %s\n"
        "// =============================================================================\n"
        "#pragma once\n" % program_name
    )
    if include_global:
        # Context header: GlobalData.h transitively provides Common.h, the
        # primitive typedefs, the address macros, and every shared type.
        head += '#include "%s"\n' % header_include_path(GLOBAL_HEADER)
        return head
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


def header_include_path(filename):
    """Repo-root-relative #include path for a generated header (CGecko adds the
    repo root to the include path)."""
    sub = HEADER_SUBDIRS.get(filename)
    if sub:
        return "Include/%s/%s" % (sub, filename)
    return "Include/%s" % filename


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


STUB_BANNER = "// --- Stubs for zero-length Ghidra types (give them a real layout to fix) ---"


def build_zero_length_stubs(dtm):
    """Ghidra zero-length composites become forward-only (incomplete) in C, so
    they can't be used as array elements or by value. Emit a 1-byte stub for
    each, BEFORE the main types, so those uses compile. (These are placeholders;
    give them a real layout in Ghidra to fix the size.)
    Returns ([(name, stub_line)], set_of_names)."""
    pairs = []
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
            pairs.append((nm, "%s %s { unsigned char _opaque_stub; } __attribute__((packed));"
                          % (kw, nm)))
    return pairs, seen


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

def collect_data_symbols(program, names, addr_filter=None):
    """Labeled, defined data -> (offset, "#define ... ") tuples. Scalars use
    VAR_ADDRESS(type, addr); N-D arrays use ARRAY_1D_ADDRESS..ARRAY_5D_ADDRESS
    against the base element type (6D+ flattens to 1D with a warning). The offset
    lets the caller route each symbol to GlobalData.h (below the shared cutoff)
    or the context header (at/above it). addr_filter(offset)->bool, when given,
    limits which symbols are exported (skipped ones claim no names)."""
    out = []
    listing = program.getListing()
    items = []
    di = listing.getDefinedData(True)
    while di.hasNext():
        data = di.next()
        sym = data.getPrimarySymbol()
        if sym is None or not is_exportable_name(sym.getName()):
            continue
        if addr_filter is not None and not addr_filter(data.getMinAddress().getOffset()):
            continue
        items.append((data.getMinAddress(), sym.getName(), data.getDataType()))
    items.sort(key=lambda t: t[0])
    for addr, name, dt in items:
        name = names.claim(sanitize_ident(name), addr)
        if isinstance(dt, Array):
            dims = []
            cur = dt
            while isinstance(cur, Array):
                dims.append(cur.getNumElements())
                cur = cur.getDataType()
            elem = sanitize_type_str(cur.getDisplayName())
            if len(dims) > 5:
                total = 1
                for d in dims:
                    total *= d
                print("[WARN] %s: %dD array flattened to [%d]; Common.h supports up to 5D"
                      % (name, len(dims), total))
                dims = [total]
            dims_str = ", ".join(str(d) for d in dims)
            line = ("#define %s ARRAY_%dD_ADDRESS(%s, %s, %s)"
                    % (name, len(dims), elem, dims_str, addr_hex(addr)))
        else:
            elem = sanitize_type_str(dt.getDisplayName())
            line = "#define %s VAR_ADDRESS(%s, %s)" % (name, elem, addr_hex(addr))
        out.append((addr.getOffset(), name, line))
    return out


def collect_plain_labels(program, names, addr_filter=None):
    """Labels with no defined data -> (offset, "#define NAME_ADDR 0x..") tuples."""
    out = []
    st = program.getSymbolTable()
    listing = program.getListing()
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
        if addr_filter is not None and not addr_filter(addr.getOffset()):
            continue
        items.append((addr, sym.getName()))
    items.sort(key=lambda t: t[0])
    for addr, name in items:
        name = names.claim(sanitize_ident(name) + "_ADDR", addr)
        out.append((addr.getOffset(), name, "#define %s %s" % (name, addr_hex(addr))))
    return out


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


def collect_functions(program, names, addr_filter=None):
    """Named functions -> (offset, wrapper-or-macro line) tuples. Emits a static
    inline wrapper per function (parameter names de-conflicted against the macro
    namespace); true varargs functions fall back to the FUNCTION_ADDRESS macro
    since a wrapper body can't forward '...'. The offset routes each function to
    GlobalData.h or the context header."""
    out = []
    fm = program.getFunctionManager()
    items = []
    for fn in fm.getFunctions(True):
        if fn.isThunk() or fn.isExternal():
            continue
        if not is_exportable_name(fn.getName()):
            continue
        if addr_filter is not None and not addr_filter(fn.getEntryPoint().getOffset()):
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
            line = ("#define %s FUNCTION_ADDRESS(%s, %s, %s)"
                    % (name, ret, addr_hex(addr), ", ".join(cast_list + ["..."])))
            out.append((addr.getOffset(), name, line))
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
        out.append((addr.getOffset(), name,
                    "static inline %s %s(%s) { %s }" % (ret, name, sig_str, body)))
    return out


# ==============================================================================
# MAIN
# ==============================================================================

# ==============================================================================
# SHARED-DOL SPLIT  (GlobalData.h + per-REL context headers + reconciliation)
# ==============================================================================

def split_type_blocks(text):
    """Rewritten type text -> list of (name, block_text) for each top-level item.
    name is the defined type name, or None for #defines / comments / blank / other
    lines. Anonymous `enum {}` blocks are paired with the following
    `typedef <int> NAME;` so the enum takes that NAME."""
    lines = text.split('\n')
    n = len(lines)
    blocks = []
    i = 0
    while i < n:
        line = lines[i]
        s = line.strip()
        if s == '' or s.startswith('//') or s.startswith('/*'):
            blocks.append((None, line)); i += 1; continue
        opener = re.match(r'^(?:typedef\s+)?(struct|union|enum)\b[^{;]*\{', line)
        if opener:
            kind = opener.group(1)
            depth = line.count('{') - line.count('}')
            buf = [line]; i += 1
            while i < n and depth > 0:
                buf.append(lines[i])
                depth += lines[i].count('{') - lines[i].count('}')
                i += 1
            block_text = '\n'.join(buf)
            nm = re.match(r'^(?:typedef\s+)?(?:struct|union|enum)\s+(\w+)', line)
            name = nm.group(1) if nm else None
            if kind == 'enum' and name is None:
                j = i
                while j < n and lines[j].strip() == '':
                    j += 1
                if j < n:
                    tm = re.match(r'^typedef\s+.*\b(\w+)\s*;\s*$', lines[j])
                    if tm:
                        name = tm.group(1)
                        block_text = block_text + '\n' + '\n'.join(lines[i:j + 1])
                        i = j + 1
            blocks.append((name, block_text)); continue
        fwd = re.match(r'^typedef\s+(?:struct|union|enum)\s+(\w+)\b', s)
        if fwd and '{' not in line:
            blocks.append((fwd.group(1), line)); i += 1; continue
        plain = re.match(r'^typedef\s+.*\b(\w+)\s*;\s*$', s)
        if plain:
            blocks.append((plain.group(1), line)); i += 1; continue
        blocks.append((None, line)); i += 1; continue
    return blocks


def keep_only_type_blocks(text, keep_names):
    """Keep only the type blocks whose defined name is in keep_names; drop
    everything else (shared types, #defines, primitives, comments). Used to emit
    the OTHER program's type "top-up": types it has that GlobalData.h lacks."""
    out = []
    for name, block in split_type_blocks(text):
        if name is not None and name in keep_names:
            out.append(block)
    return '\n'.join(out)


def _norm_block(text):
    """Collapse whitespace so trivial formatting differences don't read as drift."""
    return re.sub(r'\s+', ' ', text).strip()


def collect_shared_index(program, cutoff):
    """Below-cutoff symbols of `program` as {offset: (name, kind, type_str)} for
    the reconciliation report. Covers defined data (kind 'data', type = Ghidra
    display name), functions ('func', type = prototype), and labels ('label').
    Raw Ghidra names (not sanitized), so the report points at what's in the tool."""
    idx = {}
    listing = program.getListing()
    di = listing.getDefinedData(True)
    while di.hasNext():
        data = di.next()
        addr = data.getMinAddress()
        if addr.getOffset() >= cutoff:
            continue
        sym = data.getPrimarySymbol()
        if sym is None or not is_exportable_name(sym.getName()):
            continue
        idx[addr.getOffset()] = (sym.getName(), 'data',
                                 data.getDataType().getDisplayName())
    fm = program.getFunctionManager()
    for fn in fm.getFunctions(True):
        if fn.isThunk() or fn.isExternal() or not is_exportable_name(fn.getName()):
            continue
        off = fn.getEntryPoint().getOffset()
        if off >= cutoff:
            continue
        idx[off] = (fn.getName(), 'func', fn.getSignature().getPrototypeString())
    st = program.getSymbolTable()
    it = st.getSymbolIterator()
    while it.hasNext():
        sym = it.next()
        if sym.getSymbolType() != SymbolType.LABEL or not sym.isPrimary():
            continue
        addr = sym.getAddress()
        if not addr.isMemoryAddress() or addr.getOffset() >= cutoff:
            continue
        if listing.getDefinedDataAt(addr) is not None:
            continue
        if not is_exportable_name(sym.getName()):
            continue
        off = addr.getOffset()
        if off not in idx:
            idx[off] = (sym.getName(), 'label', '')
    return idx


# ---- shared-region merge -----------------------------------------------------
# The DOL (below-cutoff) region is the same memory in both projects, but labels
# were added independently. Auto names (FUN_/DAT_/...) are already filtered out,
# so every indexed entry is a "real" label; the merge decides which program's
# version of each address is exported into GlobalData.h.

KIND_RANK = {'label': 0, 'data': 1, 'func': 2}


def _src_rank(source):
    """How deliberate a symbol/signature is: user > imported > analysis > default."""
    if source == SourceType.USER_DEFINED:
        return 3
    if source == SourceType.IMPORTED:
        return 2
    if source == SourceType.ANALYSIS:
        return 1
    return 0


def build_merge_index(program, cutoff):
    """Below-cutoff exportable symbols -> {offset: (kind, name, score)}.
    score is a per-kind quality tuple used to pick a winner when both programs
    label the same address:
      func : (name source, signature source, parameter count)
      data : (name source, 1 if the data has a real type / 0 if undefinedN)
      label: (name source,)
    If one address holds several symbol kinds, the richest kind wins the slot
    (func > data > label)."""
    idx = {}
    listing = program.getListing()
    st = program.getSymbolTable()
    it = st.getSymbolIterator()
    while it.hasNext():
        sym = it.next()
        if sym.getSymbolType() != SymbolType.LABEL or not sym.isPrimary():
            continue
        addr = sym.getAddress()
        if not addr.isMemoryAddress() or addr.getOffset() >= cutoff:
            continue
        if listing.getDefinedDataAt(addr) is not None:
            continue
        if not is_exportable_name(sym.getName()):
            continue
        idx[addr.getOffset()] = ('label', sym.getName(), (_src_rank(sym.getSource()),))
    di = listing.getDefinedData(True)
    while di.hasNext():
        data = di.next()
        addr = data.getMinAddress()
        if addr.getOffset() >= cutoff:
            continue
        sym = data.getPrimarySymbol()
        if sym is None or not is_exportable_name(sym.getName()):
            continue
        typed = 0 if data.getDataType().getDisplayName().startswith('undefined') else 1
        idx[addr.getOffset()] = ('data', sym.getName(), (_src_rank(sym.getSource()), typed))
    fm = program.getFunctionManager()
    for fn in fm.getFunctions(True):
        if fn.isThunk() or fn.isExternal() or not is_exportable_name(fn.getName()):
            continue
        off = fn.getEntryPoint().getOffset()
        if off >= cutoff:
            continue
        try:
            sig_rank = _src_rank(fn.getSignatureSource())
        except Exception:
            sig_rank = 0
        idx[off] = ('func', fn.getName(),
                    (_src_rank(fn.getSymbol().getSource()), sig_rank,
                     fn.getParameterCount()))
    return idx


def resolve_shared_winners(canon_idx, other_idx):
    """Offsets whose shared-region entry is taken from the OTHER program.
    An address labeled only in the other program always wins; when both label
    it, the richer entry wins (kind rank, then the per-kind score); a genuine
    tie goes to canonical."""
    wins = set()
    for off, (ok, _on, oscore) in other_idx.items():
        entry = canon_idx.get(off)
        if entry is None:
            wins.add(off)
            continue
        ck, _cn, cscore = entry
        if KIND_RANK[ok] != KIND_RANK[ck]:
            if KIND_RANK[ok] > KIND_RANK[ck]:
                wins.add(off)
        elif oscore > cscore:
            wins.add(off)
    return wins


def open_second_program(name):
    """Open the OTHER REL program for its context header + the diff. Uses
    askProgram (a project program picker) so it works regardless of how the
    project is laid out; GhidraScript releases the program at script exit.
    Returns the Program, or None if unavailable / cancelled."""
    try:
        prog = askProgram("Select the '%s' program (for %s + shared-region diff)"
                          % (name, resolve_output_filename(name)))
    except Exception as e:
        println("[WARN] Program picker cancelled/unavailable: %s" % e)
        return None
    if prog is None:
        println("[WARN] No second program selected.")
    return prog


def build_diff_report(canon_name, canon_idx, other_name, other_idx,
                      canon_type_blocks, other_type_blocks, topup_names,
                      other_wins=None):
    """Compare the two programs' shared (below-cutoff) regions and their shared
    type definitions. Reports disagreements; the export already resolved
    symbol-level ones (see other_wins), so this tells you what to re-sync in
    Ghidra so both projects agree."""
    other_wins = other_wins or set()
    L = []
    L.append("Shared-region reconciliation")
    L.append("  canonical : %s   (owns GlobalData.h; wins ties)" % canon_name)
    L.append("  other     : %s" % other_name)
    L.append("  cutoff    : below 0x%08X" % SHARED_CUTOFF)
    L.append("=" * 74)

    co = set(canon_idx.keys())
    oo = set(other_idx.keys())
    only_other = sorted(oo - co)
    name_mismatch = []
    type_mismatch = []
    for off in sorted(co & oo):
        cn, ck, ct = canon_idx[off]
        on, ok, ot = other_idx[off]
        if cn != on:
            name_mismatch.append((off, cn, on))
        elif ck == 'data' and ok == 'data' and ct != ot:
            type_mismatch.append((off, cn, ct, ot))

    def section(title, rows):
        L.append("")
        L.append(title)
        if rows:
            L.extend(rows)
        else:
            L.append("  (none)")

    def winner(off):
        return other_name if off in other_wins else canon_name

    section("[TYPE MISMATCH]  same address + name, different type  (DANGEROUS -- wrong layout):",
            sum(([ "  0x%08X  %s   (exported: %s)" % (off, nm, winner(off)),
                   "      %-10s %s" % (canon_name + ":", ct),
                   "      %-10s %s" % (other_name + ":", ot) ]
                 for off, nm, ct, ot in type_mismatch), []))
    section("[NAME MISMATCH]  same address, different name  (richer label was exported):",
            ["  0x%08X  %s=%s   %s=%s   (exported: %s)"
             % (off, canon_name, cn, other_name, on, winner(off))
             for off, cn, on in name_mismatch])
    section("[MERGED OVERRIDES]  labeled in both; %s's richer entry was exported:" % other_name,
            ["  0x%08X  %s (%s)" % (off, other_idx[off][0], other_idx[off][1])
             for off in sorted(other_wins & co & oo)])
    section("[%s-ONLY SYMBOLS]  labeled below cutoff only in %s  (merged into %s):"
            % (other_name, other_name, GLOBAL_HEADER),
            ["  0x%08X  %s (%s)" % (off, other_idx[off][0], other_idx[off][1])
             for off in only_other])

    canon_tb = dict((nm, b) for nm, b in canon_type_blocks if nm is not None)
    other_tb = dict((nm, b) for nm, b in other_type_blocks if nm is not None)
    drift = [nm for nm in sorted(set(canon_tb) & set(other_tb))
             if _norm_block(canon_tb[nm]) != _norm_block(other_tb[nm])]
    section("[TYPE DRIFT]  defined in both programs but with different definitions (canonical wins):",
            ["  %s" % nm for nm in drift])
    section("[%s-ONLY TYPES]  in %s, not canonical  (emitted as top-up in %s):"
            % (other_name, other_name, GLOBAL_HEADER),
            ["  %s" % nm for nm in sorted(topup_names)])

    L.append("")
    L.append("=" * 74)
    L.append("Counts: type-mismatch=%d  name-mismatch=%d  merged-overrides=%d  %s-only=%d  type-drift=%d  top-up-types=%d"
             % (len(type_mismatch), len(name_mismatch), len(other_wins & co & oo),
                other_name, len(only_other), len(drift), len(topup_names)))
    if type_mismatch or drift:
        L.append("ACTION: resolve TYPE MISMATCH / TYPE DRIFT by syncing the two Ghidra projects.")
    return "\n".join(L)


# ---- assembly / IO helpers ---------------------------------------------------

def _split_items(items):
    """Split [(offset, name, line)] at the shared cutoff. Returns (below, above),
    both still as (offset, name, line) tuples so merged sections can be
    re-sorted by address."""
    below = [t for t in items if t[0] < SHARED_CUTOFF]
    above = [t for t in items if t[0] >= SHARED_CUTOFF]
    return below, above


def _lines(items):
    return "\n".join(ln for _off, _nm, ln in items)


def _section_banner(title):
    bar = "// " + "=" * 77
    return "%s\n//  %s\n%s" % (bar, title, bar)


def _assemble(header_lines, sections):
    """sections = list of (title, body_text); skip empty bodies."""
    parts = list(header_lines)
    for title, body in sections:
        if body and body.strip():
            parts.append(_section_banner(title))
            parts.append(body)
    parts.append("")
    return "\n\n".join(parts)


def _write_file(out_dir, filename, text):
    sub = HEADER_SUBDIRS.get(filename)
    if sub:
        out_dir = os.path.join(out_dir, sub)
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
    path = os.path.join(out_dir, filename)
    fd, tmp = tempfile.mkstemp(suffix=".tmp", dir=out_dir)
    try:
        f = os.fdopen(fd, "w")
        f.write(text)
        f.close()
        if os.path.exists(path):
            os.remove(path)
        os.rename(tmp, path)
    finally:
        if os.path.exists(tmp):
            os.remove(tmp)
    return path


def _process_types(program):
    """Set the sanitize context to `program` and return its sanitized type text
    plus the metadata the callers need. MUST be called before collecting that
    program's symbols (the collectors sanitize types against this context);
    when two programs are exported, re-set the context via set_sanitize_context
    with this dict's 'enum_meta' before collecting each one's symbols."""
    dtm = program.getDataTypeManager()
    enum_meta = collect_enum_meta(dtm)
    set_sanitize_context(enum_meta, GHIDRA_TYPE_RENAMES)
    raw = build_types_raw(dtm)
    # DataTypeWriter can emit \r\n (and lone \r on Jython/Windows); normalize so
    # the generated headers have uniform line endings and GCC line numbers match.
    raw = raw.replace('\r\n', '\n').replace('\r', '\n')
    prefix_set, type_names, const_names = scan_header_names(raw)
    stub_pairs, stub_names = build_zero_length_stubs(dtm)
    decl_names = set()
    clean, dropped = rewrite_header(raw, prefix_set, enum_meta, GHIDRA_TYPE_RENAMES,
                                    PACK_STRUCTS, stub_names=stub_names,
                                    name_sink=decl_names)
    types_text = ""
    if stub_pairs:
        types_text += STUB_BANNER + "\n" + "\n".join(ln for _nm, ln in stub_pairs) + "\n\n"
    types_text += clean
    if EMIT_SIZE_GUARDS:
        types_text += "\n" + "\n".join(build_size_guards(dtm)) + "\n"
    return {
        'type_names': set(type_names) | set(stub_names),
        'const_names': set(const_names),
        'decl_names': decl_names,          # struct fields + enum constants emitted
        'enum_meta': enum_meta,
        'stub_pairs': stub_pairs,
        'types_text': types_text, 'clean': clean, 'dropped': dropped,
    }


def _resolve_out_dir(out_name):
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
    return out_dir


def _seed_registry(names, *type_infos):
    """Seed a NameRegistry with every identifier a #define must never collide
    with: Common.h/primitive names plus each program's type names, enum
    constants, and struct-field names (an object-like macro would otherwise
    mangle any later use of that identifier, e.g. a field declaration)."""
    names.seed(RESERVED_IDENTIFIERS)
    for ti in type_infos:
        if ti is None:
            continue
        names.seed(ti['type_names'])
        names.seed(ti['const_names'])
        names.seed(ti['decl_names'])


def run_single(program, program_name, out_dir):
    """Legacy single-header export (no split), for debugging one program."""
    out_name = resolve_output_filename(program_name)
    ti = _process_types(program)
    names = NameRegistry()
    _seed_registry(names, ti)
    data_lines = [ln for _o, _n, ln in collect_data_symbols(program, names)]
    label_lines = [ln for _o, _n, ln in collect_plain_labels(program, names)]
    func_lines = [ln for _o, _n, ln in collect_functions(program, names)]
    text = _assemble(
        [build_prelude(program_name)],
        [("DATA TYPES (structs / enums / typedefs)", ti['types_text']),
         ("DATA SYMBOLS (labeled globals)", "\n".join(data_lines)),
         ("PLAIN LABELS (no defined data; raw addresses)", "\n".join(label_lines)),
         ("FUNCTIONS (static inline wrappers / varargs macros)", "\n".join(func_lines))])
    path = _write_file(out_dir, out_name, text)
    println("[INFO] Wrote %s (single-header mode)" % path)


def run_shared_split(canonical, canon_name, out_dir):
    """Full split: GlobalData.h (types from both programs + the MERGED shared
    region), the canonical context header, the other program's context header,
    and the reconciliation report."""
    # ---- 0. Open the other program up front: its labels participate in the
    # shared-region merge, and its type/field/constant names must be known
    # before any #define name is claimed (an object-like macro poisons every
    # later use of that identifier, including struct fields in either header).
    other = open_second_program(OTHER_PROGRAM_NAME)
    other_name = None
    ot = None
    other_merge_idx = {}
    if other is not None:
        other_name = other.getDomainFile().getName()
        if other_name == canon_name:
            println("[WARN] You selected the canonical program ('%s') again -- continuing"
                    % canon_name)
            println("       without a second program (no merge, no %s, no report)."
                    % resolve_output_filename(OTHER_PROGRAM_NAME))
            other = None
    if other is not None:
        try:
            println("[INFO] Other program: %s" % other_name)
            ot = _process_types(other)                    # context: OTHER
            other_merge_idx = build_merge_index(other, SHARED_CUTOFF)
        except Exception as e:
            println("[ERROR] Failed reading '%s' (%s) -- exporting canonical only."
                    % (other_name, e))
            other = None
            ot = None
            other_merge_idx = {}

    # ---- 1. Canonical types + merge decision ----------------------------------
    println("[INFO] Canonical program: %s" % canon_name)
    ct = _process_types(canonical)                        # context: CANONICAL
    canon_merge_idx = build_merge_index(canonical, SHARED_CUTOFF)
    other_wins = resolve_shared_winners(canon_merge_idx, other_merge_idx)
    if other_wins:
        println("[INFO] Shared-region merge: %d symbols taken from %s (%d of them override a canonical label)"
                % (len(other_wins), other_name,
                   len([o for o in other_wins if o in canon_merge_idx])))

    # One registry for everything visible from GlobalData.h / MatchData.h, seeded
    # so no #define can collide with any identifier in either program's types.
    namesA = NameRegistry()
    _seed_registry(namesA, ct, ot)

    # Canonical symbols, minus the shared addresses the other program wins.
    keep_canon = lambda off: off not in other_wins
    c_data = collect_data_symbols(canonical, namesA, keep_canon)
    c_lbl = collect_plain_labels(canonical, namesA, keep_canon)

    # The other program's winning shared symbols (claim names BEFORE any
    # function wrappers so no later #define can mangle a wrapper's parameter).
    take_other = lambda off: off in other_wins
    m_data, m_lbl, m_fn = [], [], []
    if other is not None and ot is not None:
        set_sanitize_context(ot['enum_meta'], GHIDRA_TYPE_RENAMES)
        m_data = collect_data_symbols(other, namesA, take_other)
        m_lbl = collect_plain_labels(other, namesA, take_other)
        set_sanitize_context(ct['enum_meta'], GHIDRA_TYPE_RENAMES)
    c_fn = collect_functions(canonical, namesA, keep_canon)
    if other is not None and ot is not None:
        set_sanitize_context(ot['enum_meta'], GHIDRA_TYPE_RENAMES)
        m_fn = collect_functions(other, namesA, take_other)
        set_sanitize_context(ct['enum_meta'], GHIDRA_TYPE_RENAMES)

    d_below, d_above = _split_items(c_data)
    l_below, l_above = _split_items(c_lbl)
    f_below, f_above = _split_items(c_fn)
    d_below = sorted(d_below + m_data)
    l_below = sorted(l_below + m_lbl)
    f_below = sorted(f_below + m_fn)

    # ---- 2. GlobalData.h: both programs' types + the merged shared region -----
    # The top-up types live HERE (not in the other context header) because a
    # merged shared symbol from the other program may reference them.
    topup_names = set()
    topup_text = ""
    if ot is not None:
        topup_names = set(ot['type_names']) - set(ct['type_names'])
        topup_text = keep_only_type_blocks(ot['clean'], topup_names)
        topup_stubs = [ln for nm, ln in ot['stub_pairs'] if nm not in ct['type_names']]
        if topup_stubs:
            topup_text = STUB_BANNER + "\n" + "\n".join(topup_stubs) + "\n\n" + topup_text

    global_sections = [
        ("SHARED TYPES (structs / enums / typedefs -- from canonical '%s')" % canon_name,
         ct['types_text'])]
    if topup_text.strip():
        global_sections.append(
            ("%s-ONLY TYPES (top-up: types only that program defines)" % other_name,
             topup_text))
    global_sections += [
        ("SHARED DATA SYMBOLS (DOL, below 0x%08X; merged from both programs)" % SHARED_CUTOFF,
         _lines(d_below)),
        ("SHARED PLAIN LABELS (DOL; merged)", _lines(l_below)),
        ("SHARED FUNCTIONS (DOL; merged)", _lines(f_below))]
    global_text = _assemble([build_prelude(canon_name)], global_sections)
    _write_file(out_dir, GLOBAL_HEADER, global_text)
    println("[INFO] Wrote %s  (shared: %d data / %d labels / %d funcs; top-up types: %d)"
            % (GLOBAL_HEADER, len(d_below), len(l_below), len(f_below), len(topup_names)))

    # ---- 3. Canonical context header ------------------------------------------
    canon_ctx = resolve_output_filename(canon_name)
    ctx_text = _assemble(
        [build_prelude(canon_name, include_global=True)],
        [("%s-SPECIFIC DATA SYMBOLS (REL, at/above 0x%08X)" % (canon_name, SHARED_CUTOFF),
          _lines(d_above)),
         ("%s-SPECIFIC PLAIN LABELS (REL)" % canon_name, _lines(l_above)),
         ("%s-SPECIFIC FUNCTIONS (REL)" % canon_name, _lines(f_above))])
    _write_file(out_dir, canon_ctx, ctx_text)
    println("[INFO] Wrote %s  (above-cutoff: %d data / %d labels / %d funcs)"
            % (canon_ctx, len(d_above), len(l_above), len(f_above)))

    if other is None or ot is None:
        println("[WARN] Second program unavailable -- wrote %s + %s only (no %s, no report)."
                % (GLOBAL_HEADER, canon_ctx, resolve_output_filename(OTHER_PROGRAM_NAME)))
        return

    # ---- 4. Other program's context header -------------------------------------
    # GlobalData.h + the canonical context header are already on disk. Isolate the
    # remaining work so any surprise here still leaves those two intact.
    try:
        # Its macros must avoid every name visible through GlobalData.h. namesA
        # already holds all of those (plus MatchData.h's, which is harmlessly
        # stricter), so seed from it wholesale.
        namesB = NameRegistry()
        namesB.seed(namesA.seen)
        set_sanitize_context(ot['enum_meta'], GHIDRA_TYPE_RENAMES)
        take_above = lambda off: off >= SHARED_CUTOFF
        od_above = collect_data_symbols(other, namesB, take_above)
        ol_above = collect_plain_labels(other, namesB, take_above)
        of_above = collect_functions(other, namesB, take_above)

        other_ctx = resolve_output_filename(other_name)
        other_text = _assemble(
            [build_prelude(other_name, include_global=True)],
            [("%s-SPECIFIC DATA SYMBOLS (REL, at/above 0x%08X)" % (other_name, SHARED_CUTOFF),
              _lines(od_above)),
             ("%s-SPECIFIC PLAIN LABELS (REL)" % other_name, _lines(ol_above)),
             ("%s-SPECIFIC FUNCTIONS (REL)" % other_name, _lines(of_above))])
        _write_file(out_dir, other_ctx, other_text)
        println("[INFO] Wrote %s  (above-cutoff: %d data / %d labels / %d funcs)"
                % (other_ctx, len(od_above), len(ol_above), len(of_above)))
    except Exception as e:
        println("[ERROR] Second-program export failed after writing %s + %s: %s"
                % (GLOBAL_HEADER, canon_ctx, e))
        println("[ERROR] Those two headers are valid; only %s + the report are missing."
                % resolve_output_filename(OTHER_PROGRAM_NAME))
        return

    # Reconciliation report is auxiliary -- never let it lose a written header.
    try:
        canon_shared_idx = collect_shared_index(canonical, SHARED_CUTOFF)
        other_shared_idx = collect_shared_index(other, SHARED_CUTOFF)
        canon_type_blocks = split_type_blocks(ct['clean'])
        other_type_blocks = split_type_blocks(ot['clean'])
        report = build_diff_report(canon_name, canon_shared_idx, other_name, other_shared_idx,
                                   canon_type_blocks, other_type_blocks, topup_names,
                                   other_wins)
        _write_file(out_dir, DIFF_REPORT_NAME, report)
        println("[INFO] Wrote %s" % DIFF_REPORT_NAME)
    except Exception as e:
        println("[WARN] Diff report failed (headers are fine): %s" % e)


def main():
    program_name = currentProgram.getDomainFile().getName()

    if not ENABLE_SHARED_SPLIT:
        out_dir = _resolve_out_dir(resolve_output_filename(program_name))
        run_single(currentProgram, program_name, out_dir)
        return

    if program_name != CANONICAL_PROGRAM:
        println("[ERROR] The shared-DOL split must be run from the canonical program")
        println("        '%s', but the current program is '%s'." % (CANONICAL_PROGRAM, program_name))
        println("        Open '%s' in the code browser and run again, or set" % CANONICAL_PROGRAM)
        println("        ENABLE_SHARED_SPLIT = False to export just this program.")
        return

    out_dir = _resolve_out_dir(GLOBAL_HEADER)
    run_shared_split(currentProgram, program_name, out_dir)
    println("[INFO] Done.")


main()
