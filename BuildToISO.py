"""Bake RioModPack into the game's DOL instead of shipping it as gecko codes.

    python BuildToISO.py            # build the pack and patch sys/main.dol
    python BuildToISO.py --verify   # show what is currently patched
    python BuildToISO.py --restore  # put the pristine DOL back

TARGET is Dolphin's extracted-disc layout (sys/main.dol + files/), so the dev
loop is patch -> boot the folder -> test, with no ISO rebuild. Release builds
use GameCube Rebuilder (gcr.exe, GUI only) against the gcr-layout root.

TEST IN STOCK DOLPHIN, NOT RIO. Project Rio always applies gecko codes -- its
built-ins are not optional -- and its codehandler installs at 0x80001800 with
its code list at the top of the arena, which are exactly the regions a baked
build wants.

ALWAYS PATCHES FROM A PRISTINE COPY. The first run saves sys/main.dol.pristine
and every run rebuilds from it, so patches never stack. Every patch asserts the
original word first, so a wrong address or an already-patched DOL fails loudly.

WHAT GETS PATCHED
  1. 0x800A8278 in initRenderMode   mr r30, r3 -> addis r30, r3, -N
        Reserves N*64 KB at the top of the game's pool. That function carves the
        arena: XFB at aligned ArenaLo, GX fifo after it, then the pool
        [base .. ArenaHi] recorded at 0x803C7470/74/78. The BASE is anchored to
        ArenaLo and only the TOP comes from ArenaHi, so shortening the top moves
        nothing. VERIFIED through a REL swap into a live match: pool base, XFB
        and every menus.rel address unchanged, reserved region byte-identical.
        NOT ArenaLo -- allocating from the bottom shifts the XFB, the fifo and
        the pool base, moving every REL address and breaking all 39 REL hooks.
  2. 0x80009404 in main             mr r3, r25 -> b <frame wrapper>
        main's frame loop; this is the unconditional join both paths reach, so
        it runs once per frame. The wrapper re-executes the displaced mr.
  3. every DOL-address C2 site      <orig> -> b <its wrapper>
  4. a new text section at 0x80001810 holding the blob.

WHY 0x80001810 FOR CODE
    Swept in stock Dolphin: the 0x80001800-0x80003100 gap below the DOL image
    (6400 bytes) survives boot intact, while sections at 0x803CD400, 0x81000000
    and 0x817E0000 are all zeroed during init. 0x80001804-0x8000180B is skipped
    because Dolphin's HLE writes "STUBHAXX" there (HLE.cpp:96) -- an emulator
    artifact, not hardware, but it would corrupt the payload while testing.
    That leaves 6384 bytes; the pack is well under it today. When it stops
    fitting, the payload moves to the reserved region with a copy performed by
    the frame stub, and only LOAD_ADDR changes here.
"""
import argparse
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "RioModPack"))
sys.path.insert(0, os.path.join(HERE, "CGecko"))
from dolfile import Dol, branch          # noqa: E402
import cgecko_iso                         # noqa: E402

GAME_ROOT = r"D:\Dolphin\Games\MSSB"
DOLPHIN = r"D:\Dolphin\Dolphin Versions\Dolphin-Beta\Dolphin.exe"
DOL = os.path.join(GAME_ROOT, "sys", "main.dol")
PRISTINE = DOL + ".pristine"

PACK = os.path.join(HERE, "RioModPack", "RioModPack.c")
LOAD_ADDR = 0x80001810           # the low gap, past Dolphin's STUBHAXX

ARENA_SITE, ARENA_ORIG = 0x800A8278, 0x7C7E1B78    # mr r30, r3
RESERVE_64K = 2                                     # 128 KB at the pool top

FRAME_SITE, FRAME_ORIG = 0x80009404, 0x7F23CB78    # mr r3, r25
FRAME_INSTR = "mr r3, r25"

GAP_END = 0x80003100


def load_pristine():
    if not os.path.isfile(DOL):
        sys.exit("[ERROR] no DOL at " + DOL)
    if not os.path.isfile(PRISTINE):
        shutil.copyfile(DOL, PRISTINE)
        print("[INFO] saved pristine copy -> " + os.path.basename(PRISTINE))
    return Dol(PRISTINE)


def show(dol, label):
    print("--- %s ---" % label)
    for i, kind, off, addr, size in dol.sections():
        print("  [%2d] %-4s file 0x%06X  addr 0x%08X  size 0x%06X" % (i, kind, off, addr, size))
    print("  free slots: text %s  data %s" % (dol.free_text_slots(), dol.free_data_slots()))
    w = dol.read_word(ARENA_SITE)
    print("  arena 0x%08X: %08X   %s" % (
        ARENA_SITE, w,
        "PRISTINE" if w == ARENA_ORIG else "patched -> pool top -%d KB" % (((-w) & 0xFFFF) * 64)))
    w = dol.read_word(FRAME_SITE)
    print("  frame 0x%08X: %08X   %s" % (
        FRAME_SITE, w, "PRISTINE" if w == FRAME_ORIG else "patched -> per-frame hook"))


def build():
    print("[1/4] building %s" % os.path.relpath(PACK, HERE))
    blob, hooks, frame = cgecko_iso.build(PACK, LOAD_ADDR, FRAME_SITE, FRAME_INSTR)
    end = LOAD_ADDR + len(blob)
    print("      %d bytes at 0x%08X-0x%08X" % (len(blob), LOAD_ADDR, end))
    if end > GAP_END:
        sys.exit("[ERROR] blob overruns the low gap by %d bytes (ends 0x%08X, gap ends "
                 "0x%08X).\n        Move the payload to the reserved region and copy it "
                 "up from the frame stub." % (end - GAP_END, end, GAP_END))
    print("      %d bytes of the gap left" % (GAP_END - end))

    dol = load_pristine()

    print("[2/4] reserving memory")
    addis = (15 << 26) | (30 << 21) | (3 << 16) | ((-RESERVE_64K) & 0xFFFF)
    dol.patch_word(ARENA_SITE, addis, expect=ARENA_ORIG)
    print("      0x%08X  addis r30, r3, -%d   (%d KB at the pool top)"
          % (ARENA_SITE, RESERVE_64K, RESERVE_64K * 64))

    print("[3/4] adding the payload section")
    slot = dol.add_section(LOAD_ADDR, blob, kind="text")
    print("      text slot %d at 0x%08X" % (slot, LOAD_ADDR))

    print("[4/4] writing branches")
    dol.patch_word(FRAME_SITE, branch(FRAME_SITE, frame["wrapper"]), expect=FRAME_ORIG)
    print("      per-frame  0x%08X -> b 0x%08X   calls: %s"
          % (FRAME_SITE, frame["wrapper"], ", ".join(frame["calls"]) or "(none)"))

    deferred = []
    for h in hooks:
        if h["rel"]:
            deferred.append(h)
            continue
        dol.patch_word(h["site"], branch(h["site"], h["wrapper"]))
        print("      %-22s 0x%08X -> b 0x%08X   (static, DOL)"
              % (h["name"], h["site"], h["wrapper"]))
    for h in deferred:
        print("      %-22s 0x%08X -> b 0x%08X   (re-applied per frame, REL)"
              % (h["name"], h["site"], h["wrapper"]))

    dol.save(DOL)
    with open(os.path.join(HERE, "RioModPack", "last_build.json"), "w") as f:
        json.dump({"load_addr": LOAD_ADDR, "size": len(blob),
                   "frame": frame, "hooks": hooks}, f, indent=2)

    print("\n[OK] wrote %s (%d bytes)" % (DOL, len(dol.raw)))
    if deferred:
        print("     %d REL hook(s) are written by the per-frame re-applier rather than"
              % len(deferred))
        print("     baked in: a branch into REL space is wiped whenever a REL loads, so")
        print("     it is re-checked each frame and rewritten only when missing.")
    print('\n     test:  "%s" -e "%s" -b' % (DOLPHIN, DOL))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--verify", action="store_true")
    ap.add_argument("--restore", action="store_true")
    a = ap.parse_args()
    if a.restore:
        if not os.path.isfile(PRISTINE):
            sys.exit("[ERROR] no pristine copy")
        shutil.copyfile(PRISTINE, DOL)
        print("[OK] restored " + DOL)
        return
    if a.verify:
        show(Dol(DOL), "current sys/main.dol")
        return
    build()


if __name__ == "__main__":
    main()
