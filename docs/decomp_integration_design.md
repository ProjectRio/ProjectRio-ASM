# Sourcing game data from the decomp repo

Design for replacing the Ghidra export (`GlobalData.h`, `MatchData.h`,
`MenuData.h`) with headers generated from the MSSB decompilation repo, so that
writing a gecko code feels like writing code inside the decomp.

**Pipeline direction (one way, one source at a time):**

```
Ghidra  ->  decomp repo  ->  ProjectRio-ASM
        (existing tools)   (new SyncFromDecomp.py)
```

Ghidra changes are imported into the decomp; the ASM repo is refreshed from the
decomp only. You never reference both at once.

Everything under "Findings" was measured.

---

## 1. Findings

### 1.1 The arena region can be dropped entirely

Earlier I flagged the band between the DOL and the RELs
(`0x803CD310` – `0x8063F094`) as permanently Ghidra-only. Measured, it does not
matter:

- 131 of the 14,095 generated symbols fall in it.
- **Zero** have a hand-given name — the seven that look real
  (`Vec3f_804cebd4`, `GXTlutObj_804ceb60`, …) are Ghidra's typed-data auto-naming.
- **Zero** are referenced by any mod, by name or as a raw hex literal, across 56
  source files.

This matches the reasoning that only run-to-run stable addresses are usable: that
band is heap and loaded assets. Note that REL `.bss` is *not* in it — the game
REL's `.bss` is at `0x808610E0`, above the REL base, and is stable.

**So there is no permanent reason to keep Ghidra in the loop.** The decomp can be
the sole source.

### 1.2 Real coverage is 70%, not 24–43%

My earlier coverage numbers measured total header bulk and were misleading. Of
14,095 symbols defined across the three generated headers, **only 78 are actually
referenced by any mod** — the headers are 99.4% unused.

Of those 78, the decomp already knows **55 (70%)**. The 23 gaps have clear owners:

| gap | ~count | who should own it |
|---|---|---|
| Mid-function hook sites — `0x80640234`, `loadDemoMatch`, `checkForNewPlayer` … | 5 | The mod. These are *instruction* addresses inside functions, not symbols; no symbol table will ever have them. |
| Menu REL — `teamSelectionSetChemStars`, `cursorToStadIDMapping` | 2 | Fixable: the decomp has the menu REL, it just needs its bases recorded. |
| REL `.bss` — `playFrameCounter`, `AtBat_PitchThrown`, `lastPlayFrameCounter` | 3 | Label in the decomp. |
| DOL settings — `StadiumID`, `enableMusic`, `PlayerPorts`, `runsNeededForMercy` … | 13 | Mixed: some are Project Rio's own additions (belong to this repo); the rest want labelling in the decomp. |

### 1.3 Joining on symbol *name* works well

The decomp headers already carry the type in the declaration
(`extern InMemBallType g_Ball;`), and `config/*/symbols.txt` maps name → address.
Joining on the name avoids all address arithmetic. Measured over the decomp's
`include/` tree:

- function prototypes: **2572 / 3012 resolved (85%)**
- extern data declarations: **132 / 212 resolved (62%)**

Unresolved names cluster in the C3 graphics library headers (`actor.h`,
`shader.h`, `geoPalette.h`) — a visible, fixable list, not scattered noise.

### 1.4 Match and menu RELs load at the same address

Solved against the `AtGameSettingsScreen` snapshot: the menu REL base is
`0x8063F094`, identical to the game REL (989/1233 exact function-size matches).
They share one arena slot and are mutually exclusive.

The generated headers must therefore make it impossible to include match and menu
game headers in one code — the same address means different things depending on
which REL is resident.

### 1.5 REL section bases need one measured number per REL

REL image sections are contiguous with 8-byte alignment; verified exactly on the
game REL:

```
text 0x8063F094 + 0x16E3A4 -> align8 -> 0x807AD440 = rodata   OK
rodata 0x807AD440 + 0x041B8 -> align8 -> 0x807B1600 = data    OK
```

The same rule predicts the menu REL's `.rodata` at `0x806D5E30`, matching an
independent size-voting derivation. Only `.text` must be measured per REL, plus
`.bss` (separately allocated — the game REL's data→bss gap is 546,360 bytes).

---

## 2. Architecture

A single script in this repo, output gitignored:

```
python SyncFromDecomp.py --decomp "../MSSB Decomp"
```

It reads the decomp's `include/` tree plus `config/*/symbols.txt` and writes
`Include/GameCode/`, mirroring the decomp's own directory layout so an include
line looks like the decomp:

```c
#include "GameCode/game/UnknownHomes_Game.h"
#include "GameCode/game/rep_720.h"
```

### 2.1 Per-construct transformation

| decomp construct | emitted into `Include/GameCode/` |
|---|---|
| `typedef` / `struct` / `union` / `enum` | copied through unchanged |
| constant `#define`s, macros | copied through |
| `extern T name;` | `#define name VAR_ADDRESS(T, 0xADDR)` |
| `extern T name[N];` | `#define name ARRAY_1D_ADDRESS(T, N, 0xADDR)` (2D–5D likewise) |
| `RET fn(args);` | `static inline RET fn(args) { return ((RET(*)(casts))0xADDR)(args); }` |
| varargs prototype | `#define fn FUNCTION_ADDRESS(RET, 0xADDR, casts, ...)` — the one case a wrapper cannot express, since a wrapper body cannot forward `...` |
| name with no address | omitted, and logged to the sync report |

The `static inline` wrapper form is taken from `ExportGameData.py`'s
`collect_functions()`, so call sites keep reading exactly like decomp code —
`manageLoadingState();` — rather than going through `FUNCTION_ADDRESS`.

Because prototypes are *replaced* by wrappers (an `extern` declaration followed by
a `static inline` definition of the same name is a conflict), decomp headers
cannot be vendored verbatim. Rewriting them, as you proposed, is the correct
approach rather than a compromise.

### 2.2 Dependency shimming

`UnknownHomes_Game.h` pulls in `mssbTypes.h`, `types.h`, `Dolphin/pad.h`,
`Dolphin/mtx.h`, `math.h`. Copy the small MSSB ones through; provide thin shims
for the Dolphin SDK headers rather than importing the SDK. `types.h`'s
`typedef signed long s32` is safe — `powerpc-eabi-gcc` and CodeWarrior both use
4-byte `long` here.

### 2.3 REL context separation

`Include/GameCode/game/**` is match context; `menus/**` is menu context. Each
subtree's root header defines a context guard and `#error`s if the other is
already defined. Given §1.4 this is not pedantry.

### 2.4 What stays hand-written

Only `Include/Rio/` -- headers for things Project Rio invented (ScreenText,
ScreenList, the Dictionary reroute). Everything that describes the game comes
from the decomp:

- mid-function hook addresses (§1.2) are mod-side constants and live in the mod
- a game object or function the decomp has not labelled yet gets its type,
  extern or prototype added to the decomp first, then synced -- never a local
  copy here

`Include/Local/` (the recovered Ghidra-era types) was retired on 2026-09-02
once its last contents had been upstreamed: `Static_MSSB_Data`, `ScreenText`
(now the decomp's own `ScreenTextPool`), `cursorPositions`, `g_MatchInfo`,
`g_InputBuffer`, `aiPosSwapInputs`, the menu/fielding members and the eight
draft-flow prototypes.

---

## 3. Verifying layouts actually agree

The decomp is built by CodeWarrior, the codes by `powerpc-eabi-gcc`. Nothing
guarantees identical struct layout, and a silent disagreement produces codes that
read the wrong offsets.

`ExportGameData.py` already emits `_Static_assert` size guards behind
`EMIT_SIZE_GUARDS`; keep that and extend it. The decomp documents *every* field
offset (`/* 0x1B78 */`) and most struct sizes (`// size: 0x1BF8`), so the sync
script can turn those comments into checks nearly for free:

```c
_Static_assert(offsetof(InMemBallType, fielderWBallIndex) == 0x1B78, "layout drift");
_Static_assert(sizeof(InMemBallType) == 0x1BF8, "size drift");
```

**Do not apply `__attribute__((packed))` to decomp structs.** Ghidra structs need
it because Ghidra emits no padding; decomp structs express padding explicitly
(`artificial_padding`). Packing them would mask a real disagreement and can
pessimise codegen into unaligned accesses. Assert instead — a failure is then a
build error that also flags a genuine decomp bug.

---

## 4. Migration cost

The decomp and the Ghidra export disagree on names: 33 struct fields and 123
symbols where both have a real but different name (see the decomp's
`docs/ghidra_name_conflicts.md`). `CPU Always Sprints.c` already hits one —
`gameControls.fieldingTeam_P1_P2_` is the Ghidra name.

Only 78 symbols are actually used by mods (§1.2), so the real churn is small.
Have the sync script emit `RenameMap.txt` and offer `--apply-renames`; do it as
one mechanical commit and verify by rebuilding every code and confirming the
gecko output is byte-identical, since only names changed.

---

## 5. Order of execution

1. **Decomp:** extend `tools/cvt_rel_addr_to_mapped_addr.py` to `{text, bss}` per
   REL with `rodata`/`data` computed (§1.5), and add the `menus` REL. Useful to
   the decomp on its own merits, and it unblocks menu-context generation.
2. **ASM repo:** `SyncFromDecomp.py` — parse, resolve, emit `Include/GameCode/`
   plus a coverage report. Touch nothing else.
3. Add layout assertions (§3) and get a clean build.
4. Port one code end to end. `CPU Always Sprints.c` is the right pick: it uses
   `inMemBatter`, `inMemRunners` and `gameControls`, and hits a conflicting field
   name. Confirm byte-identical gecko output.
5. Work the §1.2 gap list — mostly labelling in the decomp, which is now the
   right place for that work.
6. Mechanical rename commit across all codes (§4).
7. Retire `ExportGameData.py`.

Steps 1–4 change no existing code and are fully reversible.

---

## 6. Deliberately out of scope

- **Local variable names** — they exist only in Ghidra's decompiler output, not in
  any symbol table.
- **`inMemCamera`** — the decomp models it as one flat `0x28A9` struct, Ghidra as
  `cameraStruct[37]` (`0x290C`); the sizes do not reconcile and the question is
  open. The sync script should support a per-struct exclusion list so this one can
  be held back without blocking everything else.
