"""Build a gecko code with CGecko and deploy it to Project Rio's USER ini only.

Project Rio reads game inis from two places -- the user directory (what the GUI
writes) and the shipped Sys directory inside the install -- and it does NOT let
one override the other: it CONCATENATES them. A code present in both is applied
TWICE, which is silent until the doubled list overruns Rio's gecko buffer; then
the list is truncated mid-code and the game hangs on boot with no error beyond a
"GeckoCodes: Using 8160 of 8160 bytes" line in the log.

Measured 2026-08-28: with this file's code in both inis, the live list contained
two copies of every injection (searched the list at 0x817FDDD0 for the code's own
`040241D4 60000000` write) and the game never reached the first frame. With one
copy the same build boots and runs.

So: build once, to the user ini, and strip any same-named code out of the Sys ini
so a stale copy left by an older build cannot double-apply.

    python BuildToRio.py "Gecko Codes/Global/Load Challenge REL.c"

Any extra arguments are passed straight through to cgecko, e.g. --disabled, -d.

Why Project Rio and not stock Dolphin: stock Dolphin writes the code list into a
FIXED region and drops whatever does not fit, logging "Too many GeckoCodes! ...
only N remain". Rio sizes the region to the content instead -- GeckoCode.cpp does

    gecko_end   = read(0x80000034)                 // ArenaHi
    total_bytes = sum(code.codes.size()) * 8
    base        = gecko_end - total_bytes - 0x10
    write(0x80000034, base)                        // lower ArenaHi to match

so THERE IS NO CODE-LIST SIZE LIMIT in Rio; the arena shrinks by exactly what the
enabled codes need.

Do NOT read the log's "GeckoCodes: Using N of M bytes" as a budget. Because the
region is sized to fit, M == N always: end-start works out to exactly total_bytes,
so the line reads 100% full no matter how much or little is enabled. An earlier
version of this file mistook one such line ("Using 8160 of 8160 bytes") for a
~8 KB cap -- it was just the size of what happened to be enabled that day. The
"Too many GeckoCodes!" branch is unreachable in Rio for the same reason.

The real cost of a big list is that ArenaHi drops, so the game's heap shrinks by
total_bytes. That is a soft, much larger limit than 8 KB, and nothing warns you
about it -- if a huge list ever caused trouble it would show up as an allocation
failure in-game, not as a truncated code list.
"""

import json
import os
import re
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CONFIG = os.path.join(SCRIPT_DIR, "config.json")
CGECKO = os.path.join(SCRIPT_DIR, "CGecko", "cgecko.py")

USER_INI = r"C:\Users\15165\Documents\Project Rio\GameSettings\GYQE01.ini"
SYS_INI = (r"C:\Users\15165\AppData\Local\Programs\Project Rio"
           r"\Sys\GameSettings\GYQE01.ini")


def code_names(ini_path: str) -> list[str]:
    """Every `$Name` heading in the ini's [Gecko] section."""
    names, in_gecko = [], False
    with open(ini_path, encoding="utf-8", errors="replace") as f:
        for line in f:
            s = line.strip()
            if s.startswith("[") and s.endswith("]"):
                in_gecko = s == "[Gecko]"
            elif in_gecko and s.startswith("$"):
                names.append(s[1:].split("[")[0].strip())
    return names


def strip_code(ini_path: str, name: str) -> bool:
    """Remove one named code from an ini: its [Gecko] block and its
    [Gecko_Enabled]/[Gecko_Disabled] toggle. Returns True if anything changed."""
    if not os.path.isfile(ini_path):
        return False
    with open(ini_path, encoding="utf-8", errors="replace") as f:
        lines = f.read().splitlines()
    out, dropping, changed = [], False, False
    for line in lines:
        s = line.strip()
        if s.startswith("[") and s.endswith("]"):
            dropping = False
        elif s.startswith("$"):
            dropping = s[1:].split("[")[0].strip() == name
            if dropping:
                changed = True
                continue
        elif dropping:
            changed = True
            continue
        out.append(line)
    if changed:
        with open(ini_path, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(out) + "\n")
    return changed


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    if not os.path.isdir(os.path.dirname(USER_INI)):
        print(f"[ERROR] no such directory for {USER_INI}")
        return 1

    with open(CONFIG, encoding="utf-8") as f:
        original = f.read()
    config = json.loads(original)
    config["ini_path"] = USER_INI

    env = dict(os.environ, PYTHONUTF8="1")   # cgecko's status output is Unicode
    try:
        with open(CONFIG, "w", encoding="utf-8") as f:
            json.dump(config, f, indent=4)
        print(f"=== deploying to {USER_INI} ===")
        result = subprocess.run([sys.executable, CGECKO, *sys.argv[1:]], env=env)
    finally:
        with open(CONFIG, "w", encoding="utf-8") as f:   # always restore
            f.write(original)
    if result.returncode != 0:
        return result.returncode

    # Anything the build just wrote to the user ini must not also sit in Sys.
    for name in code_names(USER_INI):
        if strip_code(SYS_INI, name):
            print(f"[INFO] removed duplicate '{name}' from the Sys ini "
                  f"(Rio concatenates both inis)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
