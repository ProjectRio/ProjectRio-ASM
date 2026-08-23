#!/usr/bin/env python3
"""
Build the migration map from the old Ghidra-exported headers to decomp names.

    python BuildRenameMap.py --decomp "../MSSB Decomp"          # write docs/rename_map.md
    python BuildRenameMap.py --decomp "../MSSB Decomp" --apply  # rewrite mod sources

Every mapping is anchored on the **address** the old header recorded, never on a
similar-looking name: for each symbol a mod references, look up what the decomp
calls that same address. A name-based match would be a guess; an address match
is a fact.

The old GlobalData.h / MatchData.h / MenuData.h were deleted in the reorg, so
they are read back out of git (`git show HEAD:...`).

Outcomes per symbol:

  identity   decomp calls it the same thing        -> nothing to do
  rename     decomp has a different name           -> mechanical, --apply does it
  field      the address is inside a struct the decomp knows; the old header had
             flattened that field out to its own absolute address
  unresolved decomp has nothing at or covering it  -> belongs in Include/Rio/, or
             wants labelling in the decomp

--apply only performs `rename` rows. Field and unresolved rows need a human,
because the access path changes rather than the spelling.
"""
import argparse
import collections
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
OLD_HEADERS = ("Include/Global/GlobalData.h",
               "Include/Match/MatchData.h",
               "Include/Menu/MenuData.h")
OUT = HERE / "docs" / "rename_map.md"

DOL_END, LO_REL = 0x803CD310, 0x8063F094

DEF_RE = re.compile(r"^#define\s+(\w+)\s+.*?0x([0-9A-Fa-f]{6,8})\)\s*$")
FN_RE = re.compile(r"^static inline .*?\b(\w+)\s*\(.*?0x([0-9A-Fa-f]{6,8})\)")
IDENT_RE = re.compile(r"\b[A-Za-z_]\w*\b")


def old_header_symbols():
    """{name: addr} from the pre-reorg Ghidra headers, read out of git."""
    out = {}
    for h in OLD_HEADERS:
        r = subprocess.run(["git", "-C", str(HERE), "show", "HEAD:" + h],
                           capture_output=True, text=True)
        if r.returncode:
            print("warning: cannot read %s from git (%s)" % (h, r.stderr.strip()[:80]))
            continue
        for line in r.stdout.splitlines():
            m = DEF_RE.match(line) or FN_RE.match(line)
            if m:
                out.setdefault(m.group(1), int(m.group(2), 16))
    return out


def mod_identifiers():
    used = set()
    for pat in ("Gecko Codes/**/*.c", "Gecko Codes/**/*.h", "Gecko Codes/**/*.asm",
                "C/**/*.c", "Include/Rio/*.h"):
        for p in HERE.glob(pat):
            used |= set(IDENT_RE.findall(p.read_text(encoding="utf-8", errors="replace")))
    return used


def decomp_tables(decomp):
    sys.path.insert(0, str(decomp / "tools"))
    import ghidra_query as gq
    import ghidra_rename as gr
    cs = gq.config_symbols(with_meta=True)
    by_addr = {}
    for n, (a, sec, meta, p) in cs.items():
        by_addr.setdefault(a, (n, "type:function" in meta))
    # sizes, so we can tell "inside a known object" from "nothing here"
    sizes = {}
    size_re = re.compile(r"size:0x([0-9A-Fa-f]+)")
    for n, (a, sec, meta, p) in cs.items():
        m = size_re.search(meta)
        if m:
            sizes[a] = int(m.group(1), 16)
    return gq, gr, cs, by_addr, sizes


# An offset larger than this is not a struct member in any useful sense -- it is
# an element deep inside a big table, or a symbol whose recorded size overreaches.
FIELD_OFFSET_LIMIT = 0x100


def containing(addr, by_addr, sizes):
    """Nearest preceding decomp symbol that covers addr -> (name, offset, is_fn)."""
    best = None
    for a in sizes:
        if a <= addr < a + sizes[a] and (best is None or a > best):
            best = a
    if best is None:
        return None, None, None
    name, is_fn = by_addr[best]
    return name, addr - best, is_fn


def classify(decomp, targets):
    gq, gr, cs, by_addr, sizes = decomp_tables(decomp)
    rows = []
    for name, addr in sorted(targets.items(), key=lambda kv: kv[1]):
        hit = by_addr.get(addr)
        if hit is not None:
            dname = hit[0]
            if gr.REPO_PLACEHOLDER.match(dname):
                rows.append((addr, name, dname, "placeholder", ""))
            elif dname == name:
                rows.append((addr, name, dname, "identity", ""))
            else:
                rows.append((addr, name, dname, "rename", ""))
            continue
        owner, off, is_fn = containing(addr, by_addr, sizes)
        if owner and is_fn:
            # an address inside a function body is a hook site, not a symbol
            rows.append((addr, name, owner, "hook-site", "+0x%X into %s" % (off, owner)))
        elif owner and off < FIELD_OFFSET_LIMIT:
            rows.append((addr, name, owner, "field", "+0x%X" % off))
        elif owner:
            # deep inside a big table, or the container's recorded size overreaches
            rows.append((addr, name, owner, "table-element", "+0x%X into %s" % (off, owner)))
        else:
            where = ("main DOL" if addr < DOL_END else
                     "arena (runtime/heap, never in the decomp)" if addr < LO_REL else
                     "REL .bss or unmapped")
            rows.append((addr, name, None, "unresolved", where))
    return rows


def apply_renames(rows):
    mapping = {old: new for _a, old, new, kind, _x in rows if kind == "rename"}
    if not mapping:
        return 0, 0
    rx = re.compile(r"\b(" + "|".join(sorted(map(re.escape, mapping), key=len, reverse=True)) + r")\b")
    files = hits = 0
    for pat in ("Gecko Codes/**/*.c", "Gecko Codes/**/*.h", "Gecko Codes/**/*.asm",
                "C/**/*.c", "Include/Rio/*.h"):
        for p in HERE.glob(pat):
            raw = p.read_bytes()
            text = raw.decode("utf-8", errors="replace")
            new_text, n = rx.subn(lambda m: mapping[m.group(1)], text)
            if n:
                p.write_bytes(new_text.encode("utf-8"))   # bytes: preserve CRLF
                files += 1
                hits += n
    return files, hits


def write_report(rows, path):
    by_kind = collections.defaultdict(list)
    for r in rows:
        by_kind[r[3]].append(r)
    L = ["# Ghidra -> decomp rename map\n",
         "Generated by `BuildRenameMap.py`. Every row is anchored on the address the",
         "old Ghidra header recorded, so a mapping is a fact about memory rather than",
         "a guess from similar-looking names.\n",
         "| outcome | count | action |",
         "|---|---|---|",
         "| identity | %d | none -- the decomp already agrees |" % len(by_kind["identity"]),
         "| rename | %d | mechanical, `--apply` handles it |" % len(by_kind["rename"]),
         "| field | %d | the old header flattened a struct field to an absolute address; the access path changes |" % len(by_kind["field"]),
         "| table-element | %d | deep inside a big object -- verify before trusting |" % len(by_kind["table-element"]),
         "| hook-site | %d | an address inside a function body; mod-side constant |" % len(by_kind["hook-site"]),
         "| placeholder | %d | decomp still unnamed here -- import from Ghidra first |" % len(by_kind["placeholder"]),
         "| unresolved | %d | belongs in `Include/Rio/`, or wants labelling in the decomp |" % len(by_kind["unresolved"]),
         ""]
    if by_kind["rename"]:
        L += ["## Renames\n", "| address | mod uses | decomp calls it |", "|---|---|---|"]
        L += ["| `0x%08X` | `%s` | `%s` |" % (a, o, n) for a, o, n, _k, _x in by_kind["rename"]]
        L.append("")
    if by_kind["field"]:
        L += ["## Struct fields\n",
              "The old export gave each field its own absolute address. The decomp models",
              "them as members, so these need an access path, not a rename.\n",
              "| address | mod uses | inside | offset |", "|---|---|---|---|"]
        L += ["| `0x%08X` | `%s` | `%s` | %s |" % (a, o, n, x) for a, o, n, _k, x in by_kind["field"]]
        L.append("")
    for kind, title, blurb in (
            ("table-element", "Table elements",
             "Deep inside a large object -- either a real array element, or the "
             "container's recorded size in the decomp overreaches. Check before trusting."),
            ("hook-site", "Inside a function body",
             "These are instruction addresses, not symbols -- injection sites. They "
             "belong in `Include/Rio/` as mod-side constants; no symbol table will "
             "ever name them.")):
        if by_kind[kind]:
            L += ["## " + title + "\n", blurb + "\n",
                  "| address | mod uses | where |", "|---|---|---|"]
            L += ["| `0x%08X` | `%s` | %s |" % (a, o, x) for a, o, _n, _k, x in by_kind[kind]]
            L.append("")
    if by_kind["placeholder"]:
        L += ["## Still placeholders in the decomp\n", "| address | mod uses | decomp |", "|---|---|---|"]
        L += ["| `0x%08X` | `%s` | `%s` |" % (a, o, n) for a, o, n, _k, _x in by_kind["placeholder"]]
        L.append("")
    if by_kind["unresolved"]:
        L += ["## Unresolved\n", "| address | mod uses | region |", "|---|---|---|"]
        L += ["| `0x%08X` | `%s` | %s |" % (a, o, x) for a, o, _n, _k, x in by_kind["unresolved"]]
        L.append("")
    if by_kind["identity"]:
        L += ["## Identity (%d)\n" % len(by_kind["identity"]),
              "Decomp and the old header already agree; nothing to change.\n",
              ", ".join("`%s`" % o for _a, o, _n, _k, _x in by_kind["identity"]), ""]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(L) + "\n", encoding="utf-8", newline="\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--decomp", default="../MSSB Decomp")
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()

    decomp = Path(args.decomp).expanduser().resolve()
    old = old_header_symbols()
    used = mod_identifiers()
    targets = {n: a for n, a in old.items() if n in used}
    print("old header symbols: %d | referenced by mods: %d" % (len(old), len(targets)))

    rows = classify(decomp, targets)
    counts = collections.Counter(r[3] for r in rows)
    for k in ("identity", "rename", "field", "table-element", "hook-site",
              "placeholder", "unresolved"):
        print("   %-11s %3d" % (k, counts[k]))
    write_report(rows, OUT)
    print("report: %s" % OUT.relative_to(HERE))

    if args.apply:
        files, hits = apply_renames(rows)
        print("\napplied %d renames across %d files" % (hits, files))
        print("field/unresolved rows were NOT touched -- they need a human.")


if __name__ == "__main__":
    main()
