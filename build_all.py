#!/usr/bin/env python3
"""build_all.py — Build every .c, .asm, and .ini gecko code in the repo."""

import sys
import os
import subprocess
import glob

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CGECKO     = os.path.join(SCRIPT_DIR, "cgecko.py")


def find_sources() -> list[str]:
    sources = []
    for pattern in ("**/*.c", "**/*.asm", "**/*.ini"):
        for path in glob.glob(os.path.join(SCRIPT_DIR, pattern), recursive=True):
            if path.endswith(".rewritten.c"):
                continue
            sources.append(path)
    return sorted(sources)


def main():
    sources = find_sources()
    if not sources:
        print("[INFO] No .c, .asm, or .ini source files found.")
        return

    print(f"[INFO] Building {len(sources)} file(s)...\n")

    passed: list[str] = []
    failed: list[str] = []

    for src in sources:
        rel = os.path.relpath(src, SCRIPT_DIR)
        print(f"{'─' * 60}")
        print(f"Building: {rel}")
        print(f"{'─' * 60}")
        result = subprocess.run(
            [sys.executable, CGECKO, "--no-enable", "--no-launch", src],
            stdin=subprocess.DEVNULL,
        )
        if result.returncode == 0:
            passed.append(rel)
        else:
            failed.append(rel)
        print()

    print("=" * 60)
    print(f"  {len(passed)} passed  |  {len(failed)} failed  |  {len(sources)} total")
    print("=" * 60)

    if failed:
        print("\n[FAILED]")
        for f in failed:
            print(f"  {f}")
        sys.exit(1)


if __name__ == "__main__":
    main()
