# Project Rio ASM/C

A repo for all of the gecko codes made by Project Rio. Designed to maximize convenience and efficiency for modders.

This repo contains:
- `.asm` and `.c` files of mods written for Project Rio.
- Helper files `Common.s` and `Common.h` which contain macros to assist in writing mods efficiently and easily.
- A build script `cgecko.py` which takes `.asm`/`.c` files and converts them to gecko codes.

---

## Requirements

- Python 3.10+
- [devkitPPC](https://devkitpro.org/) — see installation instructions below.

---

## Installing devkitPro

devkitPPC is part of the devkitPro toolchain suite. Download and run the installer from the [devkitPro releases page](https://github.com/devkitPro/installer/releases).

| Platform | Method |
|---|---|
| Windows | `devkitProUpdater-x.x.x.exe` |
| macOS / Linux | `devkitpro-pacman` |

After installation, install the PowerPC toolchain:

```
dkp-pacman -S devkitPPC
```

The installer sets a `DEVKITPPC` environment variable automatically. The script reads this to locate the tools — no manual path configuration needed. If the variable is missing, the script falls back to `C:\devkitpro\devkitPPC\bin`.

---

## Usage

```
python3 cgecko.py [input.c|input.asm] [-t] [-d]
```

| Argument | Description |
|---|---|
| `input` | `.c` or `.asm` source file. Optional if `build_file` is set in `config.json`. |
| `-t` | Write output to `codes.txt` instead of the Dolphin ini. |
| `-d` | Debug mode: verbose output, save build artifacts (`.o`, `.elf`, `.ld`, `.disasm.txt`, `.rewritten.c`). |

---

## config.json

Place `config.json` alongside `cgecko.py`. All fields are optional.

```json
{
    "ini_path":     "C:/Users/You/Documents/Dolphin Emulator/GameSettings/GALE01.ini",
    "build_file":   "C:/ProjectRio-ASM/Codes/MyMod/myCode.c",
    "dolphin_path": "C:/Users/You/AppData/Local/Dolphin/Dolphin.exe",
    "iso_path":     "C:/Games/GALE01.iso"
}
```

| Key | Description |
|---|---|
| `ini_path` | Dolphin GameSettings ini file. Gecko code is deployed here automatically. |
| `build_file` | Default input file. Allows double-clicking `cgecko.py` to rebuild without arguments. |
| `dolphin_path` | Path to Dolphin executable. |
| `iso_path` | Path to game ISO. |

If `ini_path`, `dolphin_path`, and `iso_path` are all set, Dolphin launches automatically after deploying the code.

If no `ini_path` is set, the script warns and falls back to writing `codes.txt` in the script directory.

---

## Dolphin ini Integration

The script reads and writes the Dolphin GameSettings ini directly:
- If the code name already exists in `[Gecko]`, its contents are updated in place.
- If the code is new, it is appended to `[Gecko]` and added to `[Gecko_Enabled]`.
- All other ini content is preserved exactly.
- A blank or missing ini file is initialized with the correct `[Gecko]` / `[Gecko_Enabled]` structure.

---

## Source File Comments

Both `.c` and `.asm` files use the same comment syntax. C files use `//`, ASM files use `#`.

| Comment | Required | Description |
|---|---|---|
| `// Address: 0x80XXXXXX` | Yes | Injection address |
| `// Author: YourName` | No | Author for gecko header |
| `// Instruction: <asm>` | No | PPC instruction at inject site — enables conditional wrapper |
| `// *Note text` | No | Note line appended after code block. Repeatable. |

The code name is derived automatically from the source filename (e.g. `myCode.c` → `myCode`).

### Gecko Output Format

```
$Name [Author]
20XXXXXX IIIIIIII   <- conditional wrapper (only if // Instruction present)
C2XXXXXX LLLLLLLL   <- C3 if address >= 0x81000000
...payload lines...
E2000001 00000000   <- conditional close
*Note text
```

---

## C Files

### Entry Function

The entry function must match the filename. For `myCode.c`, define `void myCode()`. The script:
- Moves the entry function to the top of the compiled output
- Adds `BACKUP` (saves r3–r31, LR, and any used FPRs) before it runs
- Adds `RESTORE` (reloads all saved registers) after it returns
- Replaces all `blr` instructions with forward branches to the terminator

Helper functions (any name other than the entry function) are normal C — define them freely.

### Stack Management

The script injects BACKUP/RESTORE as raw instruction bytes around the compiled payload. GCC is configured with `-ffixed-r14` through `-ffixed-r31` to prevent it from using callee-saved registers — this eliminates GCC's own sub-frame entirely, keeping the output clean and minimal.

Frame layout:

```
new SP + 0x000   back-chain
new SP + 0x008   r3   (stmw saves r3–r31)
...
new SP + 0x080   r31
new SP + 0x088   f0   (stfd fN at frame+0x88+N*8, if FPU used)
...
old SP + 0x004   LR   = new SP + frame_size + 0x4
```

### Register Access

To read or write a game register at the injection point, use the `REG()` macro from `Common.h`:

```c
void myCode() {
    REG(int, score, 20);   // score reads from r20 at injection point
    REG(int, lives, 21);   // lives reads from r21 at injection point

    score = score + 1;     // modifies r20 (note: writes not guaranteed to persist — see below)
}
```

> **Note:** `REG()` writes are not guaranteed to be seen by the game after RESTORE reloads the saved register state. Use `REG()` primarily for reading game register values. To modify a register the game will see, write directly to its memory address via `value_atAddr`.

> VSCode may show a squiggle on `REG()` syntax — ignore it, GCC compiles it correctly.

### Memory Access

```c
// Single value at address
value_atAddr(int,   0x80123456) = 10;
int x = value_atAddr(int, 0x80123456);

// 1D array
value_atAddr(byte, 4, 0x80AABBCC)[2] = 0xFF;

// Named pointer (requires * to dereference)
DECLARE_VARIABLE(int, score, 0x80123456);
*score = 10;
int x = *score;
```

### Game Function Calls

```c
// Inline — best for one-off calls
function_atAddr(void, 0x800c836C, int, int, int, int)(soundID, 127, 0x3f, 0x0);
```

### Floats and Arrays

Float literals and static arrays in `.rodata`/`.data` require position-independent addressing since the payload address is unknown at load time. Direct `float x = 1.5f` style declarations generate absolute address relocations that the script will reject.

**Workarounds:**

Use an integer bit-cast (zero overhead, no memory):
```c
union { unsigned int i; float f; } u = {0x3FC00000};  // 1.5f
float x = u.f;
```

Use `value_atAddr` to read from a known static address in game memory:
```c
float x = value_atAddr(float, 0x80SOMEADDR);
```

### Global / Static Data and the PIC Stub

When `.rodata` or `.data` sections are non-empty, the script prepends a 4-instruction PC-relative self-location stub. After the stub, `r11` holds the runtime address of the stub itself. Use `r11 + offset` to reach data symbols. Build with `-d` and inspect `.disasm.txt` for exact offsets.

---

## ASM Files

- Include `Common.s` at the top of every `.asm` file.
- Assembled with `powerpc-eabi-as` (`-mregnames -mbig`).
- Always raw — no BACKUP/RESTORE injection.
- Same linker, relocation check, C2/C3 selection, conditional wrapper, and output format as C files.
- Comment character is `#`.

### Register Aliases

Use `.set` to give a register a meaningful name:

```asm
.set pPlayer, r3
.set score,   r4
.set temp,    r12

lwz  score, 0x44(pPlayer)
addi score, score, 1
stw  score, 0x44(pPlayer)
```

---

## Debug Artifacts (`-d`)

| File | Description |
|---|---|
| `.rewritten.c` | C source after entry function reordering (C only) |
| `.o` | Compiled object file |
| `.elf` | Linked ELF at base address 0x0 |
| `.ld` | Linker script used |
| `.disasm.txt` | objdump disassembly |

---

## Full C Template

```c
#include "Common.h"

// Author:      YourName
// Address:     0x80XXXXXX
// Instruction: li r3, 0          (optional — enables conditional wrapper)
// *This is a note                (optional — repeatable)

// Game memory
DECLARE_VARIABLE(int, score, 0x80123456);
DECLARE_VARIABLE(int, lives, 0x8012ABCD);

// Game functions (types only in parameter list — no parameter names)
#define PlaySound function_atAddr(void, 0x800c836C, int, int, int, int)

// Optional helpers
static int clamp(int val, int max) {
    return val > max ? max : val;
}

// Entry function — name must match filename (e.g. myCode.c -> myCode())
void myCode() {
    REG(int, currentScore, 20);   // read r20 at injection point

    *score = clamp(currentScore + 1, 9999);
    *lives = *lives - 1;
    PlaySound(0x1, 127, 0x3f, 0x0);
}
```

## Full ASM Template

```asm
.include "Common.s"

# Author:      YourName
# Address:     0x80XXXXXX
# Instruction: li r3, 0
# *This is a note

.set pPlayer, r3
.set score,   r4

    backupall
    lwz  score, 0x44(pPlayer)
    addi score, score, 1
    stw  score, 0x44(pPlayer)
    restoreall
```