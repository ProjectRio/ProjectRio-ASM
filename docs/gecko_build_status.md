# Gecko code build status

**All 54 codes build: 30 `.asm`, 6 `.ini`, 18 `.c`.** (`python CGecko/build_all.py`)

Point `ini_path` in `config.json` at a scratch file before a bulk rebuild -- cgecko
deploys every successful build into whatever ini that names.

---

## What had been wrong

`Include/Global/GlobalData.h`, `Include/Match/MatchData.h` and
`Include/Menu/MenuData.h` -- the Ghidra export the mods were written against --
were deleted in favour of decomp-derived headers, and 16 of the 18 `.c` mods
stopped compiling. The decomp knew almost everything they needed; it just could
not hand it over. Three separate gaps, fixed in three ways.

### 1. Symbols the decomp knew but never declared

A symbol only reached a mod if some decomp header declared it, and most were
declared nowhere -- `g_Scores`, `g_Fielders`, `screenTextArray` and
`Static_Stats_Tables` all had addresses sitting in `config/GYQE01/**/symbols.txt`
and were invisible.

`SyncFromDecomp.py` now emits every one of them into **`Include/Symbols/`** as a
raw address:

```c
#define g_Scores_ADDR   0x808928A0  // size 0xC8
```

2,044 bindings across `dol.h`, `game.h` and `menus.h`; the address only, never a
guessed type. `game.h` and `menus.h` carry a context guard, since those two RELs
share one arena slot.

### 2. Types -- moved into the decomp where the layout could be verified

The first cut of this put the missing types in `Include/Local/`. Most of them now
live in the decomp instead, because that is where the information belongs and
because `SyncFromDecomp.py` then emits them properly typed on its own:

| added to the decomp | where | evidence |
|---|---|---|
| `GameScoresControlsStruct` + `ScoreStruct`, `extern g_Scores` | `include/game/UnknownHomes_Game.h` | size 0xC8 matches `symbols.txt` exactly |
| `InMemFielder` (`pos`, `AI_Ind`, `autoMovementFunctionIndex`), `extern g_Fielders[9]` | same | 9 x 0x268 == the 0x15A8 `symbols.txt` records; `+0x1D3` also matches the mod's own note |
| `RUNNER_MOVEMENT_TYPE`, `DASH_STATE`, `AUTO_MOVEMENT_FUNCTION_INDEX` | same | values from `enum_map.md` |
| `P2_CPU_CODE` + `GameInitVariables.p2_CPU_match_code` | `include/static/UnknownHomes_Static.h` | `+0x10`, previously inside `artificial_padding` |
| `InningSettings` + `extern inningSetting` | same | `symbols.txt` size 0x60; field offsets anchored on the addresses in `rename_map.md` |

Layouts came from Ghidra's own type export (`.ghidra_cache/in_game.types.txt`),
which states every field's offset and size, so nothing here is inferred. Only the
fields in use are named; the rest is explicit padding, and the decomp still
builds byte-identical (12.61%, 1,515 functions, zero regressions).

Four things the mods reached for turned out to be **already in the decomp** and
just needed the member path: `FrameCountWhileNotAtMainMenu`, `StadiumID`,
`home_AwaySetting` (was `batsFirstSetting`) and `PlayerPorts` are all members of
`g_d_GameSettings`.

### 2b. What is still in `Include/Local/`

Addresses alone were not enough: mods do `Static_Stats_Tables.captainSelectedID[0]`
and cast to `controllerInputStruct*`. Those types exist nowhere in the decomp, so
they were recovered from the old export -- with their dependency closure -- into
**`Include/Local/`**, which `SyncFromDecomp.py` never touches:

| file | scope | holds |
|---|---|---|
| `Legacy.h` | DOL | `Static_MSSB_Data`, `ScreenText`, `TextCharacter`, `controllerInputStruct`, `structCharSelect`, `StatisticsBatter`, `GameScoresControlsStruct`; the flattened-field addresses; enum constants; call-through bindings |
| `LegacyGame.h` | match REL | `InMemFielder`, `globalFielding`, and the `g_Scores` / `g_Fielders` / `g_FieldingLogicLegacy` bindings |
| `LegacyMenus.h` | menu REL | `cursorToStadIDMapping`, `changeScreenVariables`, `copyInfoToInMemRoster` |

Where the decomp also declares a type, the recovered copy is renamed
`Legacy_<name>` so both can be in scope without colliding.

**Why these have not moved yet.** Their field names come from an older state of
the Ghidra project than the one `.ghidra_cache/in_game.types.txt` was exported
from, and the two do not fully reconcile. `ScreenText` is the clearest case: both
agree it is 0x38 bytes and agree on `xPos`, `yPos` and `color`, but the six tail
fields the mods use (`textStyle_`, `lineSpacing_`, `alighment_left_center_right`,
`currLetterBeingDrawn`, `field17_0x2a`, `field18_0x2b`) are unnamed in the current
export. Writing the old names into the decomp would put layout there that cannot
be checked against the live project -- the thing `file_map.md` warns about. The
fix is to name those fields in Ghidra and re-export, then they move like the
others did.

### 3. Names the decomp spells differently

Straight renames, taken from `rename_map.md` / `field_map.md` / `enum_map.md`:
`_6_FramesUntilNotSprinting` -> `FramesUntilNotSprinting`, `VecXYZ.X/Y/Z` ->
`.x/.y/.z`, `CharacterStats.Stats` -> `.stats`.

Fields the old export had flattened into their own absolute address are kept in
`Legacy.h` with the derivation recorded, e.g.

```c
#define rel VAR_ADDRESS(short, 0x800E877C)   // inningSetting + 0x28
```

---

## Three judgement calls worth knowing about

**`teamIsCPU`.** Ghidra put `teamIsCPU[2]` at `GameControlsStruct + 0x13E`; the
decomp declares `baseRoundingState` and `overrun1st_doneChecking` at those two
bytes. They cannot both be right, and `field_map.md` already flags the offset.
`CPU vs CPU.c` now writes through `GameLogic_teamIsCPU`, which preserves exactly
what the mod did before rather than silently adopting the decomp's reading.
Whoever settles this should delete the binding.

**`fielderControl`.** Same shape: the decomp models `0x80892750` as
`g_FieldingLogic_s` with no `fielderControl` member; the export modelled it as
`globalFielding`, whose first member *is* `fielderControl`. `g_FieldingLogicLegacy`
keeps the export's shape over the same bytes.

**Unprototyped calls.** Eight DOL functions and two menu functions have an
address and no signature anywhere, so they are bound as unprototyped pointers
returning `int` (`Legacy.h` section 5). Arguments are unchecked. Correct when the
result is an integer or ignored; write the `FUNCTION_ADDRESS` call out by hand if
you need a real signature.

**Boot To Match** runs across the menu -> match handoff, so it needs an object
from the match REL while bound to the menu REL's symbols. The context guard
rightly refuses to bind both, so `g_Scores` is bound by address in the mod itself
and is only valid once the match REL is resident.

---

## Compiling is not running

Every code now assembles into gecko output. **None of this was tested in-game.**
`CPU Always Sprints.c` still carries its author's `// WIP, doesn't work yet`, and
the two shape conflicts above are exactly the kind of thing that builds cleanly
and behaves wrongly. Test before shipping any of them.

---

## One CGecko bug, still open

`CGecko/cgecko.py:639` -- `die()` guards its prompt with `sys.stdin.isatty()`, but
on Windows that returns **True** for the NUL device, so any batch run with
`stdin=DEVNULL` (including CGecko's own `build_all.py`) reaches `input()`, raises
`EOFError`, and the top-level handler calls `die()` again for a second one. The
exit code stays 1 so pass/fail tallies are right; the cost is that the real error
ends up buried under two tracebacks. A `try/except EOFError` around the `input()`
fixes every caller.
