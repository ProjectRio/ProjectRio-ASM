#!/usr/bin/env python3
"""
Map struct field names from the old Ghidra-exported headers to the decomp's.

    python BuildFieldMap.py --decomp "../MSSB Decomp"          # write docs/field_map.md
    python BuildFieldMap.py --decomp "../MSSB Decomp" --apply   # rewrite mod sources

Anchoring, in two steps, neither of which guesses from names:

1. **Which struct is which** comes from the symbols. The old header records the
   type of every global (`#define inMemBall VAR_ADDRESS(InMemBall, 0x80890B38)`)
   and the decomp records its own (`extern InMemBallType g_Ball;`). Both sit at
   the same address, so `InMemBall` and `InMemBallType` are the same struct --
   a fact about memory, not a similar-looking name.

2. **Which field is which** comes from the offset. Ghidra's type dump gives each
   member's offset; the decomp writes them in `/* 0x1B78 */` comments. Equal
   offset in a confirmed pair means the same field.

A rename is only emitted when the old name is unambiguous across all pairs --
`fielderControl` meaning one thing in one struct and something else in another
would be rewritten wrongly by a blind text substitution, so those are reported
instead of applied.
"""
import argparse
import collections
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "docs" / "field_map.md"
OLD_HEADERS = ("Include/Global/GlobalData.h",
               "Include/Match/MatchData.h",
               "Include/Menu/MenuData.h")

PRIMITIVE = {"byte", "int", "short", "long", "char", "float", "double", "uint",
             "ushort", "ulong", "undefined", "undefined1", "undefined2",
             "undefined4", "undefined8", "u8", "u16", "u32", "u64", "s8", "s16",
             "s32", "s64", "f32", "f64", "BOOL", "bool", "void", "code"}

VAR_ADDR = re.compile(
    r"^#define\s+(\w+)\s+(?:VAR|ARRAY_\dD)_ADDRESS\(\s*([A-Za-z_]\w*)\s*[,\)].*?0x([0-9A-Fa-f]{6,8})\)")
EXTERN = re.compile(r"^\s*extern\s+([A-Za-z_]\w*)\s*\**\s*(\w+)\s*(?:\[[^\]]*\])?\s*;")
REPO_FIELD = re.compile(
    r"/\*\s*(0x[0-9A-Fa-f]+)\s*\*/\s*"
    r"(?:const\s+|volatile\s+|struct\s+)*"
    r"(?:E\(\s*[\w\s\*]+,\s*\w+\s*\)|[\w \*]+?)\s*(\**)\s*(\w+)\s*((?:\[[^\]]*\])*)\s*;")


def git_show(path):
    return subprocess.run(["git", "-C", str(HERE), "show", "HEAD:" + path],
                          capture_output=True, text=True).stdout


def type_pairs(decomp):
    """[(ghidra struct, decomp struct, example symbol)] anchored on addresses."""
    sys.path.insert(0, str(decomp / "tools"))
    import ghidra_query as gq
    old = {}
    for h in OLD_HEADERS:
        for line in git_show(h).splitlines():
            m = VAR_ADDR.match(line)
            if m:
                old.setdefault(m.group(1), (m.group(2), int(m.group(3), 16)))
    by_addr = {}
    for n, a in gq.config_symbols().items():
        by_addr.setdefault(a, n)
    dec_type = {}
    for h in (decomp / "include").rglob("*.h"):
        for line in h.read_text(encoding="utf-8", errors="replace").splitlines():
            m = EXTERN.match(line)
            if m:
                dec_type.setdefault(m.group(2), m.group(1))
    seen = {}
    for oname, (otype, addr) in old.items():
        d = by_addr.get(addr)
        if not d or d not in dec_type:
            continue
        dtype = dec_type[d]
        if otype in PRIMITIVE or dtype in PRIMITIVE:
            continue
        seen.setdefault((otype, dtype), oname)
    return [(o, d, s) for (o, d), s in sorted(seen.items())]


def ghidra_structs(decomp):
    """{struct: {offset: field name}} from the Ghidra type dump."""
    out = collections.defaultdict(dict)
    cur = None
    path = decomp / ".ghidra_cache" / "in_game.types.txt"
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(("STRUCT\t", "UNION\t")):
            cur = line.split("\t")[1]
        elif line.startswith("ENUM\t"):
            cur = None
        elif line.startswith("\t") and cur:
            p = line.split("\t")
            if len(p) >= 5 and p[4]:
                out[cur][int(p[1], 16)] = p[4]
    return out


def decomp_struct(decomp, name):
    """{offset: field name} for a decomp struct."""
    end = re.compile(r"^\}\s*" + re.escape(name) + r"\s*;", re.M)
    for h in list((decomp / "include").rglob("*.h")):
        text = h.read_text(encoding="utf-8", errors="replace")
        m = end.search(text)
        if not m:
            continue
        start = text.rfind("typedef struct", 0, m.start())
        if start < 0:
            continue
        out = {}
        for f in REPO_FIELD.finditer(text[start:m.start()]):
            out[int(f.group(1), 16)] = f.group(3)
        return out
    return None


def mod_identifiers():
    used = set()
    for pat in ("Gecko Codes/**/*.c", "Gecko Codes/**/*.h", "Gecko Codes/**/*.asm",
                "C/**/*.c", "Include/Rio/*.h"):
        for p in HERE.glob(pat):
            used |= set(re.findall(r"\b[A-Za-z_]\w*\b",
                                   p.read_text(encoding="utf-8", errors="replace")))
    return used


def export_name(name):
    """Reproduce the old Ghidra export's identifier sanitising: every character
    that is illegal in C became a single underscore."""
    return re.sub(r"[^A-Za-z0-9_]", "_", name)


def build(decomp):
    gh = ghidra_structs(decomp)
    used = mod_identifiers()
    rows, misses = [], []
    for gname, dname, via in type_pairs(decomp):
        gfields = gh.get(gname)
        dfields = decomp_struct(decomp, dname)
        if not gfields or not dfields:
            misses.append((gname, dname, via,
                           "no Ghidra struct" if not gfields else "no decomp struct"))
            continue
        for off, gfield in sorted(gfields.items()):
            if not gfield:
                continue
            # Mods reference the name the old export emitted, which had every
            # illegal C character replaced by "_": Ghidra's `battingTeam(P1/P2)`
            # reaches the mod as `battingTeam_P1_P2_`. Matching on the raw Ghidra
            # spelling silently drops every field whose name needed sanitising.
            asused = gfield if gfield in used else export_name(gfield)
            if asused not in used:
                continue
            dfield = dfields.get(off)
            rows.append((gname, dname, off, asused, dfield, via))
    return rows, misses


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--decomp", default="../MSSB Decomp")
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()
    decomp = Path(args.decomp).expanduser().resolve()

    rows, misses = build(decomp)
    same = [r for r in rows if r[3] == r[4]]
    diff = [r for r in rows if r[4] and r[3] != r[4]]
    gone = [r for r in rows if not r[4]]

    # a name is only safely substitutable if it maps to one target everywhere
    targets = collections.defaultdict(set)
    for _g, _d, _o, gf, df, _v in diff:
        targets[gf].add(df)
    safe = {gf: next(iter(t)) for gf, t in targets.items() if len(t) == 1}
    ambiguous = {gf: t for gf, t in targets.items() if len(t) > 1}

    print("struct pairs compared : %d (%d unusable)" % (
        len({(r[0], r[1]) for r in rows}), len(misses)))
    print("mod-used fields matched by offset: %d" % len(rows))
    print("   identical name : %d" % len(same))
    print("   renamed        : %d  (%d unambiguous, %d ambiguous)"
          % (len(diff), len(safe), len(ambiguous)))
    print("   no field at that offset in the decomp: %d" % len(gone))

    L = ["# Struct field map\n",
         "Generated by `BuildFieldMap.py`. Struct identity comes from the address of a",
         "global that has the type; field identity comes from the offset inside it.",
         "Neither step matches on names.\n",
         "| old field | use instead | struct | offset |", "|---|---|---|---|"]
    for g, d, off, gf, df, _v in sorted(diff, key=lambda r: (r[1], r[2])):
        mark = "" if gf in safe else "  **(ambiguous)**"
        L.append("| `%s` | `%s`%s | `%s` -> `%s` | 0x%X |" % (gf, df, mark, g, d, off))
    if gone:
        L += ["\n## No field at that offset in the decomp\n",
              "The decomp struct has nothing declared here -- likely a gap in its layout.\n",
              "| old field | struct | offset |", "|---|---|---|"]
        L += ["| `%s` | `%s` -> `%s` | 0x%X |" % (r[3], r[0], r[1], r[2])
              for r in sorted(gone, key=lambda r: (r[1], r[2]))]
    if ambiguous:
        L += ["\n## Ambiguous\n",
              "The same old name means different things in different structs, so a",
              "plain text substitution would corrupt one of them. Fix these by hand.\n"]
        L += ["- `%s` -> %s" % (k, ", ".join("`%s`" % x for x in sorted(v)))
              for k, v in sorted(ambiguous.items())]
    if misses:
        L += ["\n## Pairs that could not be compared\n", "| ghidra | decomp | via | why |", "|---|---|---|---|"]
        L += ["| `%s` | `%s` | `%s` | %s |" % m for m in misses]
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(L) + "\n", encoding="utf-8", newline="\n")
    print("report: %s" % OUT.relative_to(HERE))

    # --- type renames -------------------------------------------------------
    used = mod_identifiers()
    gh_all = ghidra_structs(decomp)
    tren = []
    for g, d, via in type_pairs(decomp):
        if g == d or g not in used:
            continue
        # the pair is only trustworthy if BOTH sides really are structs; a
        # Ghidra enum typed onto an address the decomp calls a struct is a
        # mismatch, not a rename (EnumControllerInput -> lbl_803C77B8_s)
        if g not in gh_all or decomp_struct(decomp, d) is None:
            continue
        tren.append((g, d, via))
    if tren:
        L += ["\n## Type renames\n", "| old type | use instead | via symbol |", "|---|---|---|"]
        L += ["| `%s` | `%s` | `%s` |" % r for r in tren]
    print("type renames usable: %d" % len(tren))

    if args.apply:
        if tren:
            tmap = {g: d for g, d, _v in tren}
            # word-anchored: unanchored, `InMemRunner` would also rewrite
            # the inside of `InMemRunnerType`, producing `InMemRunnerTypeType`
            trx = re.compile(r"\b(" + "|".join(
                sorted(map(re.escape, tmap), key=len, reverse=True)) + r")\b")
            tf = th = 0
            for pat in ("Gecko Codes/**/*.c", "Gecko Codes/**/*.h", "C/**/*.c", "Include/Rio/*.h"):
                for pp in HERE.glob(pat):
                    raw = pp.read_bytes(); text = raw.decode("utf-8", errors="replace")
                    nt, n = trx.subn(lambda m: tmap[m.group(1)], text)
                    if n:
                        pp.write_bytes(nt.encode("utf-8")); tf += 1; th += n
            print("applied %d type renames across %d files" % (th, tf))
        if not safe:
            print("nothing unambiguous to apply")
            return
        rx = re.compile(r"(?<=\.)\b(" + "|".join(sorted(map(re.escape, safe), key=len, reverse=True))
                        + r")\b|(?<=->)\b(" + "|".join(sorted(map(re.escape, safe), key=len, reverse=True)) + r")\b")
        files = hits = 0
        for pat in ("Gecko Codes/**/*.c", "Gecko Codes/**/*.h", "C/**/*.c", "Include/Rio/*.h"):
            for p in HERE.glob(pat):
                raw = p.read_bytes()
                text = raw.decode("utf-8", errors="replace")
                new, n = rx.subn(lambda m: safe[m.group(1) or m.group(2)], text)
                if n:
                    p.write_bytes(new.encode("utf-8"))
                    files += 1
                    hits += n
        print("\napplied %d field renames across %d files" % (hits, files))
        print("only names reached through `.` or `->` were touched, and only")
        print("unambiguous ones; ambiguous names were left for a human.")


if __name__ == "__main__":
    main()
