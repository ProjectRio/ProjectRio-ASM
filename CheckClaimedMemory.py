"""Enforce ClaimedFreeMemory.h.

    python CheckClaimedMemory.py            # exit 1 on any finding
    python CheckClaimedMemory.py --quiet    # only print findings

The ledger is a hand-written list of every word of otherwise-free RAM a mod or
Rio itself has taken. It only protects anyone if it is actually checked, so
this script turns it into two hard rules:

  1. No two claims overlap. Rio's own words (GameID, the desync checksum, the
     port bytes) are entries like any other, so a new mod claim that lands on
     one of them fails here instead of desyncing netplay.

  2. Every address literal in the claimable region that appears in mod CODE is
     inside some claim. Comments are ignored. A literal that is not claimed is
     either a forgotten ledger entry or a typo -- both are findings.

BuildToISO.py runs this before building, and CI runs it on every push.
"""
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LEDGER = os.path.join(HERE, "ClaimedFreeMemory.h")

# The free block the ledger governs (lbl_802EAF80 .. the live node at 0x802EC8F0)
# plus the superstar bytes it also records. Literals outside these are not
# checked: they are game objects, not claims.
CHECKED_RANGES = [(0x802EAF80, 0x802ECFC0)]

# Source trees scanned for literals.
SCAN_DIRS = ["Gecko Codes", "RioModPack", "Include/Rio"]

ENTRY_RE = re.compile(r'^\s*"0x([0-9A-Fa-f]{8})"\s*:\s*"\(([^)]*)\)\s*--\s*(.*)"')
SIZE_RE = re.compile(r"^\s*(\d+)\s*bytes")
LITERAL_RE = re.compile(r"\b0x802E[ABCabc][0-9A-Fa-f]{3}\b")


def parse_ledger(path):
    claims = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for lineno, line in enumerate(f, 1):
            m = ENTRY_RE.match(line)
            if not m:
                continue
            addr, spec, desc = int(m.group(1), 16), m.group(2).strip(), m.group(3)
            ms = SIZE_RE.match(spec)
            if ms:
                size = int(ms.group(1))
            else:
                size = {"w": 4, "h": 2, "b": 1}.get(spec.split(",")[0].strip())
            if size is None:
                raise SystemExit("%s:%d: cannot read a size out of '(%s)'" % (path, lineno, spec))
            claims.append((addr, size, desc[:60], lineno))
    return claims


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return "\n".join(l.split("//")[0] for l in text.splitlines())


def code_literals():
    found = {}
    for d in SCAN_DIRS:
        for pattern in ("**/*.c", "**/*.h"):
            for path in glob.glob(os.path.join(HERE, d, pattern), recursive=True):
                with open(path, "rb") as f:
                    text = f.read().decode("utf-8", "replace")
                for m in LITERAL_RE.finditer(strip_comments(text)):
                    found.setdefault(int(m.group(0), 16), set()).add(os.path.relpath(path, HERE))
    return found


def in_checked_range(addr):
    return any(lo <= addr < hi for lo, hi in CHECKED_RANGES)


def main():
    quiet = "--quiet" in sys.argv
    claims = sorted(parse_ledger(LEDGER))
    findings = []

    prev = None
    for c in claims:
        if prev and c[0] < prev[0] + prev[1]:
            findings.append("overlap: 0x%08X (%d bytes, %s) runs into 0x%08X (%d bytes, %s)"
                            % (prev[0], prev[1], prev[2], c[0], c[1], c[2]))
        if not prev or c[0] + c[1] > prev[0] + prev[1]:
            prev = c

    lits = code_literals()
    for addr in sorted(lits):
        if not in_checked_range(addr):
            continue
        if not any(a <= addr < a + s for a, s, _, _ in claims):
            findings.append("unclaimed: 0x%08X used in %s but not in ClaimedFreeMemory.h"
                            % (addr, ", ".join(sorted(lits[addr]))))

    if not quiet:
        print("ClaimedFreeMemory.h: %d claims, %d code literals in the claimable region checked"
              % (len(claims), sum(1 for a in lits if in_checked_range(a))))
    for f in findings:
        print("[ERROR] " + f)
    if findings:
        sys.exit(1)
    if not quiet:
        print("[OK] no overlaps, every code literal is claimed")


if __name__ == "__main__":
    main()
