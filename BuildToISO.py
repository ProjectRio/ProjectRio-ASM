"""Bake RioModPack into the game's DOL instead of shipping it as gecko codes.

    python BuildToISO.py            # build the pack and patch sys/main.dol
    python BuildToISO.py --verify   # show what is currently patched
    python BuildToISO.py --restore  # put the pristine DOL back

    python BuildToISO.py --gcr      # ... against the GameCube Rebuilder folder
    python BuildToISO.py --gcr --verify

THERE ARE TWO EXTRACTED-DISC LAYOUTS AND THEY ARE NOT INTERCHANGEABLE.

  dolphin (default)  sys/main.dol + files/
      What `Dolphin.exe -e <folder>` boots, so this is the dev loop: patch ->
      boot the folder -> test, with no ISO rebuild. Booting the folder runs the
      REAL Nintendo apploader out of sys/apploader.img, so it enforces the same
      rules a burned ISO does.

  gcr (--gcr)        &&systemdata/Start.dol, with the files at the root
      GameCube Rebuilder's layout. Only gcr.exe reads it -- Dolphin cannot boot
      this folder, it wants sys/ + files/ -- so it is for cutting a release ISO
      and nothing else. Build here when you are about to run gcr.exe, and go
      back to the default for testing.

Each target keeps its own .pristine copy next to its own DOL, so switching
between them never mixes a patched DOL from one into the other.

RUNS UNDER PROJECT RIO TOO, gecko codes and all -- verified 2026-08-29, and the
reason it did not before is gone: the payload used to sit at 0x80001810, right
where Rio's codehandler installs. Now that it lives at PAYLOAD_ADDR the low gap
is free and the two coexist. Booted under Rio with the codehandler at
0x80001800: payload intact, the baked frame hook still ours, baked C0 bodies
running, and both enabled gecko codes demonstrably applied (a C2 at 0x80034CEC
branching into Rio's code list, and an 04 write landing at 0x8063F964).

  Two caveats when mixing:
  * A gecko code that hooks a site this build also hooks wins, because the
    codehandler re-applies the whole list every frame while the baked branch is
    written once. `Instant Randoms` is a C2 at 0x80009404 -- the exact per-frame
    site -- so enabling it silently kills every baked C0. The gecko `Options
    Menu` is a C2 at 0x80658D98, the same site as the baked one; run one or the
    other, not both. Nothing in the 107-code library touches the two
    reservation patches.
  * Rio lowers ArenaHi to fit its code list (0x817FDEF8-0x817FFDC0 for a 7.8 KB
    list, so ArenaHi 0x817FDEF0). The pool cap here is ABSOLUTE, so it assumes
    ArenaHi stays above PAYLOAD_ADDR. That holds with ~32x margin today. If a
    code list ever pushes ArenaHi below the end of the payload, the loader stub
    notices at boot, puts every DOL patch back and boots the stock game (see
    build_stub -- it stamps GUARD_MARK_ADDR so the refusal is visible). Build
    with --test-arena-guard to exercise that path.

ALWAYS PATCHES FROM A PRISTINE COPY. The first run saves sys/main.dol.pristine
and every run rebuilds from it, so patches never stack. Every patch asserts the
original word first, so a wrong address or an already-patched DOL fails loudly.

WHAT GETS PATCHED
  1. 0x8006D658 in OSInit           mr r30, r3 -> lis r30, PAYLOAD_ADDR>>16
        OSInit clears the whole arena --

            memset(OSGetArenaLo(), 0, OSGetArenaHi() - OSGetArenaLo());

        -- and it runs after the loader stub, so without this the payload is
        copied up and then immediately zeroed. r30 is the top of that range and
        nothing else reads it, so capping it at PAYLOAD_ADDR wipes everything
        below the reserved region exactly as before and skips the region
        itself. (OSInit has three other clear paths for development consoles,
        which honour a protected window at 0x8000005C/60; retail hardware and
        Dolphin both take this one.)
  2. 0x800A8278 in initRenderMode   mr r30, r3 -> lis r30, PAYLOAD_ADDR>>16
        Caps the game's pool at an ABSOLUTE address instead of ArenaHi, which
        reserves everything above it. That function carves the arena: XFB at
        aligned ArenaLo, GX fifo after it, then the pool [base .. cap] recorded
        at 0x803C7470/74/78. The BASE is anchored to ArenaLo and only the TOP
        comes from ArenaHi, so lowering the top moves nothing. VERIFIED live:
        with the cap at 0x81780000 the XFB (0x803DD320), fifo (0x80469320) and
        pool base (0x804CEC80) all read back byte-identical to a stock boot.
        NOT ArenaLo -- allocating from the bottom shifts the XFB, the fifo and
        the pool base, moving every REL address and breaking all 39 REL hooks.
        Absolute rather than `addis r30, r3, -N`: the payload has to be LINKED
        at its final address, so that address must be known at build time and
        not inherited from whatever ArenaHi happens to be.
  3. the DOL entry point            0x80003154 -> STAGE_ADDR (the loader stub)
        The stub copies the payload up into the reserved region and jumps to
        the real __start. It runs before any game code, so the staging area is
        still pristine and the destination is still untouched.
  4. 0x80009404 in main             mr r3, r25 -> b <frame wrapper>
        main's frame loop; this is the unconditional join both paths reach, so
        it runs once per frame. The wrapper re-executes the displaced mr.
  5. every DOL-address C2 site      <orig> -> b <its wrapper>
  6. two new sections: the stub at STAGE_ADDR and the payload image after it.

WHY THE PAYLOAD IS STAGED AND COPIED
    The reserved region is the only significant free RAM that does not move
    anything, but a DOL section cannot be loaded into it. The retail apploader
    refuses the whole DOL if any section reaches 0x81200000:

        APPLOADER ERROR >>> One of the sections in the dol file exceeded its
        boundary. All the sections should not exceed 0x81200000 (development
        mode).

    and warns above 0x80700000 ("production mode"). So the image ships in a
    section at STAGE_ADDR, under both thresholds, and the stub copies it to
    PAYLOAD_ADDR at boot. Staging lands inside what later becomes the pool,
    which is fine: initRenderMode has not run yet, so nothing owns that memory
    until long after the copy.

    Everything else about the payload is unchanged -- it is still linked at one
    fixed address with absolute addressing, it is just linked at PAYLOAD_ADDR
    and copied there rather than loaded there.

    The stub is a TEXT section so the apploader invalidates icache over it; the
    image is a DATA section, which keeps it out of the Wii-detection scan
    DolReader runs across text sections.

THE MEMORY MAP THIS ASSUMES (measured on a stock boot, GYQE01)
    0x80003100  game image .. 0x803CD300, then bss to 0x803CD30E
    0x803DD320  XFB          <- aligned ArenaLo
    0x80469320  GX fifo
    0x804CEC80  pool base
    PAYLOAD_ADDR .. 0x817FFDA0   reserved by patches 1 and 2
    0x817FDDA0  BI2 (8 KB, read by OSInit through the pointer at 0x800000F4)
    0x817FFDA0  ArenaHi
    The payload may not reach BI2 -- the copy happens before OSInit reads it --
    so the usable window is PAYLOAD_ADDR .. 0x817FDDA0, and the build asserts it.

HOW BIG PAYLOAD_ADDR SHOULD BE
    The pool's total size is not the constraint. Its live free counter sits at
    0x803C7898 (+0 free, +4 bottom watermark, +8 top watermark), and in a real
    match it bottoms out around 1.75 MB free on a stock build -- the game very
    nearly fills its own heap while playing. So the reserve comes out of ~1.75
    MB, not out of 19 MB. 0x817C0000 takes 255 KB of that, about 14%, and gives
    247 KB of payload room (39x the old low-gap limit). Every 64 KB lower is
    another 64 KB off a margin the game is already close to using, so lower it
    only as far as a build actually needs, and re-measure a match if you do.

    0x80001800-0x80003100 (6384 bytes) is no longer used by this build. It
    survives boot intact and is still the right home for anything that needs a
    permanently mapped low address.
"""
import argparse
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "RioModPack"))
sys.path.insert(0, os.path.join(HERE, "CGecko"))
from dolfile import Dol, branch          # noqa: E402
import cgecko as cg                       # noqa: E402
import cgecko_iso                         # noqa: E402

DOLPHIN = r"D:\Dolphin\Dolphin Versions\Dolphin-Beta\Dolphin.exe"

# One entry per extracted-disc layout; see the TARGET section of the docstring.
# `dol` is relative to `root`, and the pristine copy always sits beside it.
TARGETS = {
    "dolphin": {
        "root": r"D:\Dolphin\Games\MSSB",
        "dol":  os.path.join("sys", "main.dol"),
        "desc": "Dolphin extracted folder (sys/ + files/)",
        "boot": True,                      # Dolphin can -e this folder
    },
    "gcr": {
        "root": r"D:\Dolphin\Games\MSSB\RioModPack",
        "dol":  os.path.join("&&systemdata", "Start.dol"),
        "desc": "GameCube Rebuilder folder (&&systemdata/)",
        "boot": False,                     # gcr.exe only -- Dolphin cannot boot it
    },
}
DEFAULT_TARGET = "dolphin"

# Set by select_target() before anything touches the disc. They start on the
# default so that importing this module, or calling build() directly, behaves
# the way running it with no arguments does.
TEST_ARENA_GUARD = False         # --test-arena-guard: build a DOL that always refuses (see build_stub)
TARGET = DEFAULT_TARGET
GAME_ROOT = TARGETS[TARGET]["root"]
DOL = os.path.join(GAME_ROOT, TARGETS[TARGET]["dol"])
PRISTINE = DOL + ".pristine"


def select_target(name):
    """Point the module's paths at one of TARGETS."""
    global TARGET, GAME_ROOT, DOL, PRISTINE
    TARGET = name
    GAME_ROOT = TARGETS[name]["root"]
    DOL = os.path.join(GAME_ROOT, TARGETS[name]["dol"])
    PRISTINE = DOL + ".pristine"

PACK = os.path.join(HERE, "RioModPack", "RioModPack.c")

# ---- where the payload lives -------------------------------------------------
PAYLOAD_ADDR = 0x817C0000        # final home; also the new pool cap (lis-encodable)
PAYLOAD_LIMIT = 0x817FDDA0       # BI2 starts here -- the payload must end below it
STAGE_ADDR = 0x80500000          # loader stub, then the payload image after it
STAGE_LIMIT = 0x80700000         # the apploader's "production mode" warning line

MR_R30_R3 = 0x7C7E1B78                             # mr r30, r3
WIPE_SITE = 0x8006D658                             # OSInit's arena-clear bound
ARENA_SITE = 0x800A8278                            # initRenderMode's pool cap
ENTRY_ORIG = 0x80003154                            # __start

FRAME_SITE, FRAME_ORIG = 0x80009404, 0x7F23CB78    # mr r3, r25
FRAME_INSTR = "mr r3, r25"

# Measured on a stock boot; only used to report what the reserve costs.
STOCK_POOL_BASE, STOCK_ARENA_HI = 0x804CEC80, 0x817FFDA0
# The number that actually constrains PAYLOAD_ADDR. The pool keeps its own
# free-space counter at 0x803C7898, and during a live match it bottomed out at
# 1.25 MB with 511 KB already reserved -- so a stock match runs with roughly
# this much to spare, not the 19 MB the pool's total size suggests. Measured
# 2026-08-29 on the default Boot To Match spec; other stadiums may sit lower,
# so treat it as an order of magnitude, not a budget to spend to the last byte.
MATCH_SPARE_STOCK = 1313936 + 523680


def assemble_block(asm_text):
    """Assemble a multi-instruction PPC block (labels allowed) to bytes.

    Hand-encoding the stub would be the only place in this build where an
    instruction is not produced by a real assembler, so it goes through gas
    like everything else."""
    tmpdir = tempfile.mkdtemp(prefix="iso_stub_")
    src = os.path.join(tmpdir, "stub.s")
    obj = os.path.join(tmpdir, "stub.o")
    binp = os.path.join(tmpdir, "stub.bin")
    with open(src, "w") as f:
        f.write(".text\n" + asm_text + "\n")
    r = subprocess.run([cg.AS] + cg.AS_FLAGS + [src, "-o", obj],
                       capture_output=True, text=True)
    if r.returncode != 0:
        cg.die("Failed to assemble the loader stub:\n" + r.stderr)
    r = subprocess.run([cg.OBJCOPY, "-O", "binary", "--only-section", ".text", obj, binp],
                       capture_output=True)
    if r.returncode != 0 or not os.path.isfile(binp):
        cg.die("Failed to extract the loader stub bytes.")
    with open(binp, "rb") as f:
        return f.read()


# Where the stub records that it refused to install the pack, and what it
# writes there. Read it from a memory viewer when a Rio build boots stock.
GUARD_MARK_ADDR = 0x800030FC     # in the low gap that survives boot (see the docstring)
GUARD_MARK_WORD = 0x41524E41     # 'ARNA': the arena was too small


def build_stub(src_addr, dst_addr, nbytes, entry, restore, table_addr, limit):
    """Copy nbytes from src to dst, make the copy executable, jump to entry --
    unless the arena is already too small for the payload, in which case put
    every DOL patch back and boot the stock game instead.

    THE GUARD. Project Rio lowers ArenaHi (0x80000034) to make room for its
    gecko code list at the top of memory, and the pool cap this build installs
    is absolute (PAYLOAD_ADDR). A code list big enough to push ArenaHi below the
    end of the payload would have Rio's list and this payload overwriting each
    other. So before anything is copied the stub compares ArenaHi with `limit`
    (the payload's end address); if ArenaHi is lower it walks `restore` -- the
    (address, original word) table for every DOL site this build patched --
    writes the stock words back, flushes them, stamps GUARD_MARK_WORD at
    GUARD_MARK_ADDR so the refusal is visible, and jumps to __start. Nothing of
    the pack runs; the REL hooks never get applied because the per-frame
    re-applier lives in the payload that was never copied.

    Runs as the DOL entry point, i.e. before __start's `bl __init_registers`,
    so there is no stack and no r2/r13 -- it touches only r0/r3/r4/r5, CTR and
    CR0. LR is untouched; __start clobbers it immediately anyway.

    The word count is exact and the cache-line count is rounded up, so the
    flush may cover a few bytes past the image. That is harmless: those bytes
    are ours too (the reserved region runs to 0x817FFDA0).

    Returns (bytes, code_length): the restore table follows the code, so the
    caller can compute its address from STAGE_ADDR + code_length."""
    words = (nbytes + 3) // 4
    lines = (nbytes + 31) // 32

    def hi(v):
        return (v >> 16) & 0xFFFF

    def lo(v):
        return v & 0xFFFF

    code = assemble_block("""
        lis     3, 0x8000
        lwz     3, 0x34(3)
        lis     4, {limhi}
        ori     4, 4, {limlo}
        cmplw   3, 4
        bge     0f

        lis     3, {thi}
        ori     3, 3, {tlo}
        li      5, {nres}
        mtctr   5
    4:  lwz     4, 0(3)
        lwz     0, 4(3)
        stw     0, 0(4)
        dcbst   0, 4
        sync
        icbi    0, 4
        addi    3, 3, 8
        bdnz    4b
        sync
        isync
        lis     4, {mhi}
        ori     4, 4, {mlo}
        lis     0, {vhi}
        ori     0, 0, {vlo}
        stw     0, 0(4)
        lis     5, {ehi}
        ori     5, 5, {elo}
        mtctr   5
        bctr

    0:  lis     3, {shi}
        ori     3, 3, {slo}
        lis     4, {dhi}
        ori     4, 4, {dlo}
        lis     5, {whi}
        ori     5, 5, {wlo}
        mtctr   5
    1:  lwz     0, 0(3)
        stw     0, 0(4)
        addi    3, 3, 4
        addi    4, 4, 4
        bdnz    1b

        lis     4, {dhi}
        ori     4, 4, {dlo}
        lis     5, {lhi}
        ori     5, 5, {llo}
        mtctr   5
    2:  dcbst   0, 4
        addi    4, 4, 32
        bdnz    2b
        sync

        lis     4, {dhi}
        ori     4, 4, {dlo}
        lis     5, {lhi}
        ori     5, 5, {llo}
        mtctr   5
    3:  icbi    0, 4
        addi    4, 4, 32
        bdnz    3b
        sync
        isync

        lis     5, {ehi}
        ori     5, 5, {elo}
        mtctr   5
        bctr
    """.format(shi=hi(src_addr), slo=lo(src_addr),
               dhi=hi(dst_addr), dlo=lo(dst_addr),
               whi=hi(words), wlo=lo(words),
               lhi=hi(lines), llo=lo(lines),
               ehi=hi(entry), elo=lo(entry),
               limhi=hi(limit), limlo=lo(limit),
               thi=hi(table_addr), tlo=lo(table_addr), nres=len(restore),
               mhi=hi(GUARD_MARK_ADDR), mlo=lo(GUARD_MARK_ADDR),
               vhi=hi(GUARD_MARK_WORD), vlo=lo(GUARD_MARK_WORD)))
    table = b"".join(struct.pack(">II", a, w) for a, w in restore)
    return code + table, len(code)


def load_pristine():
    if not os.path.isfile(DOL):
        sys.exit("[ERROR] no DOL at %s   (target %r = the %s)"
                 % (DOL, TARGET, TARGETS[TARGET]["desc"]))
    if not os.path.isfile(PRISTINE):
        shutil.copyfile(DOL, PRISTINE)
        print("[INFO] saved pristine copy -> " + os.path.basename(PRISTINE))
    return Dol(PRISTINE)


def show(dol, label):
    print("--- %s ---" % label)
    for i, kind, off, addr, size in dol.sections():
        print("  [%2d] %-4s file 0x%06X  addr 0x%08X  size 0x%06X" % (i, kind, off, addr, size))
    print("  free slots: text %s  data %s" % (dol.free_text_slots(), dol.free_data_slots()))
    print("  entry 0x%08X   %s" % (
        dol.entry, "PRISTINE" if dol.entry == ENTRY_ORIG else "patched -> loader stub"))
    for site, what in ((WIPE_SITE, "OSInit arena clear stops at"),
                       (ARENA_SITE, "pool capped at")):
        w = dol.read_word(site)
        print("  0x%08X: %08X   %s" % (
            site, w, "PRISTINE" if w == MR_R30_R3 else "patched -> %s 0x%04X0000"
            % (what, w & 0xFFFF)))
    w = dol.read_word(FRAME_SITE)
    print("  frame 0x%08X: %08X   %s" % (
        FRAME_SITE, w, "PRISTINE" if w == FRAME_ORIG else "patched -> per-frame hook"))


def build():
    check = subprocess.run([sys.executable, os.path.join(HERE, "CheckClaimedMemory.py"), "--quiet"])
    if check.returncode:
        sys.exit("[ERROR] ClaimedFreeMemory.h has findings (above); fix the ledger before building")

    print("[1/5] building %s" % os.path.relpath(PACK, HERE))
    blob, hooks, frame = cgecko_iso.build(PACK, PAYLOAD_ADDR, FRAME_SITE, FRAME_INSTR)
    end = PAYLOAD_ADDR + len(blob)
    room = PAYLOAD_LIMIT - PAYLOAD_ADDR
    print("      %d bytes, linked at 0x%08X-0x%08X" % (len(blob), PAYLOAD_ADDR, end))
    if end > PAYLOAD_LIMIT:
        sys.exit("[ERROR] the payload overruns the reserved region by %d bytes (ends "
                 "0x%08X, BI2 starts 0x%08X).\n        Lower PAYLOAD_ADDR -- every 64 KB "
                 "costs 64 KB of the game's pool." % (end - PAYLOAD_LIMIT, end, PAYLOAD_LIMIT))
    print("      %d of %d bytes of the reserved region used (%.1f%%)"
          % (len(blob), room, 100.0 * len(blob) / room))

    dol = load_pristine()

    print("[2/5] building the loader stub")
    # Every DOL word this build changes, with its stock value, so the stub can
    # put the game back if the arena turns out to be too small (see build_stub).
    restore = [(WIPE_SITE, MR_R30_R3), (ARENA_SITE, MR_R30_R3), (FRAME_SITE, FRAME_ORIG)]
    restore += [(h["site"], dol.read_word(h["site"])) for h in hooks if not h["rel"]]
    stub, code_len = build_stub(0, 0, len(blob), ENTRY_ORIG, restore, 0, 0)   # sized pass
    table_addr = STAGE_ADDR + code_len
    image_addr = (STAGE_ADDR + len(stub) + 31) & ~31
    guard_limit = 0x81800000 if TEST_ARENA_GUARD else end   # above any ArenaHi: always refuses
    stub, code_len2 = build_stub(image_addr, PAYLOAD_ADDR, len(blob), ENTRY_ORIG,
                                 restore, table_addr, guard_limit)
    assert code_len2 == code_len and image_addr == (STAGE_ADDR + len(stub) + 31) & ~31, \
        "stub changed size"
    stage_end = image_addr + len(blob)
    print("      %d bytes at 0x%08X; image staged at 0x%08X-0x%08X"
          % (len(stub), STAGE_ADDR, image_addr, stage_end))
    print("      arena guard: boots stock (and stamps 0x%08X) if ArenaHi < 0x%08X; "
          "%d DOL words in the restore table"
          % (GUARD_MARK_ADDR, guard_limit, len(restore)))
    if TEST_ARENA_GUARD:
        print("      *** --test-arena-guard: this DOL will ALWAYS take the refusal path ***")
    if stage_end > STAGE_LIMIT:
        sys.exit("[ERROR] staging ends at 0x%08X, past the apploader's 0x%08X production "
                 "line.\n        Lower STAGE_ADDR." % (stage_end, STAGE_LIMIT))

    print("[3/5] reserving memory")
    lis_r30 = (15 << 26) | (30 << 21) | ((PAYLOAD_ADDR >> 16) & 0xFFFF)
    dol.patch_word(WIPE_SITE, lis_r30, expect=MR_R30_R3)
    dol.patch_word(ARENA_SITE, lis_r30, expect=MR_R30_R3)
    reserved = STOCK_ARENA_HI - PAYLOAD_ADDR
    stock_pool = STOCK_ARENA_HI - STOCK_POOL_BASE
    print("      0x%08X  lis r30, 0x%04X   OSInit's arena clear stops at 0x%08X"
          % (WIPE_SITE, PAYLOAD_ADDR >> 16, PAYLOAD_ADDR))
    print("      0x%08X  lis r30, 0x%04X   pool capped at 0x%08X"
          % (ARENA_SITE, PAYLOAD_ADDR >> 16, PAYLOAD_ADDR))
    print("      reserves %d bytes; the pool goes %d -> %d bytes (-%.1f%%)"
          % (reserved, stock_pool, stock_pool - reserved,
             100.0 * reserved / stock_pool))
    print("      that is %.0f%% of the ~%.2f MB a live match actually leaves spare"
          % (100.0 * reserved / MATCH_SPARE_STOCK,
             MATCH_SPARE_STOCK / 1048576.0))

    print("[4/5] adding sections")
    slot = dol.add_section(STAGE_ADDR, stub, kind="text")
    print("      text slot %d at 0x%08X   loader stub" % (slot, STAGE_ADDR))
    slot = dol.add_section(image_addr, blob, kind="data")
    print("      data slot %d at 0x%08X   payload image" % (slot, image_addr))
    dol.set_entry(STAGE_ADDR)
    print("      entry 0x%08X -> 0x%08X   (stub, then b 0x%08X)"
          % (ENTRY_ORIG, STAGE_ADDR, ENTRY_ORIG))

    print("[5/5] writing branches")
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
        json.dump({"load_addr": PAYLOAD_ADDR, "size": len(blob),
                   "stage_addr": STAGE_ADDR, "image_addr": image_addr,
                   "stub_size": len(stub), "restore_table": len(restore),
                   "guard_mark": GUARD_MARK_ADDR, "frame": frame, "hooks": hooks}, f, indent=2)

    print("\n[OK] wrote %s (%d bytes)" % (DOL, len(dol.raw)))
    if deferred:
        print("     %d REL hook(s) are written by the per-frame re-applier rather than"
              % len(deferred))
        print("     baked in: a branch into REL space is wiped whenever a REL loads, so")
        print("     it is re-checked each frame and rewritten only when missing.")
    if TARGETS[TARGET]["boot"]:
        # -e the sys/main.dol path, not the folder: Dolphin sees the sibling
        # sys/ + files/ and boots the whole extracted disc through the real
        # apploader. This is the form actually known to work here.
        print('\n     test:  "%s" -e "%s" -b' % (DOLPHIN, DOL))
    else:
        print("\n     This is the %s. Dolphin CANNOT boot it -- it wants"
              % TARGETS[TARGET]["desc"])
        print("     sys/ + files/. Open %s in gcr.exe to build the release" % GAME_ROOT)
        print("     ISO, then drop --gcr to get back to a folder you can test.")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--verify", action="store_true",
                    help="show what is currently patched, do not build")
    ap.add_argument("--restore", action="store_true",
                    help="put this target's pristine DOL back")
    ap.add_argument("--gcr", action="store_true",
                    help="operate on the GameCube Rebuilder folder instead of the "
                         "Dolphin one. Release builds only -- Dolphin cannot boot "
                         "that layout.")
    ap.add_argument("--test-arena-guard", action="store_true",
                    help="build a DOL whose loader stub always takes the refusal path, "
                         "to test it; never ship this")
    a = ap.parse_args()
    global TEST_ARENA_GUARD
    TEST_ARENA_GUARD = a.test_arena_guard

    select_target("gcr" if a.gcr else DEFAULT_TARGET)
    print("[INFO] target: %s -- %s" % (TARGET, DOL))

    if a.restore:
        if not os.path.isfile(PRISTINE):
            sys.exit("[ERROR] no pristine copy at " + PRISTINE)
        shutil.copyfile(PRISTINE, DOL)
        print("[OK] restored " + DOL)
        return
    if a.verify:
        if not os.path.isfile(DOL):
            sys.exit("[ERROR] no DOL at %s   (target %r = the %s)"
                     % (DOL, TARGET, TARGETS[TARGET]["desc"]))
        show(Dol(DOL), "current " + os.path.relpath(DOL, GAME_ROOT))
        return
    build()


if __name__ == "__main__":
    main()
