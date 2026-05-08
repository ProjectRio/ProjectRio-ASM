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
python3 cgecko.py [input.c|input.asm] [-d]
```

| Argument | Description |
|---|---|
| `input` | `.c` or `.asm` source file. Optional if `build_file` is set in `config.json`. |
| `-d` | Debug mode: verbose output, save build artifacts. |

Output goes to the Dolphin ini if `ini_path` is set in `config.json`, otherwise falls back to `codes.txt` in the script directory.

---

## config.json

Copy `config.template.json` to `config.json` alongside `cgecko.py` and fill in the fields.

```json
{
    "build_file": "",
    "ini_path": "",
    "dolphin_path": "",
    "iso_path": ""
}
```

| Key | Description |
|---|---|
| `build_file` | Default input file. Allows double-clicking `cgecko.py` to rebuild without arguments. |
| `ini_path` | Dolphin GameSettings ini file. Gecko code is deployed here automatically. |
| `dolphin_path` | Path to Dolphin executable. |
| `iso_path` | Path to game ISO. |

If `ini_path`, `dolphin_path`, and `iso_path` are all set, Dolphin launches automatically after deploying. If `ini_path` is not set, the output falls back to `codes.txt`.

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

## ASM vs C — When to Use Which

**Use ASM when:**
- The mod is simple.
- You need precise control over registers.
- You're want the smallest possible code size.

**Use C when:**
- The mod is complex.
- You do not need precise control over registers.
- You want code that is readbale & easy to write/debug.

*Note on registers: in C, the tool will automatically backup and restore all registers. This means that you cannot overwrite any registers after running your code, making the code safe. The `REG` macro allows you to read previous registers but you cannot write to them. Some mods may require changing specific registers, and in this case you would need to use ASM instead of C.*

---

## ASM Files

Include `Common.s` at the top of every `.asm` file for access to the standard macros.

- Assembled with `powerpc-eabi-as` (`-mregnames -mbig`).
- Always raw — no automatic BACKUP/RESTORE injection. Manage the stack yourself using the macros in `Common.s`.
- Comment character is `#`.

### Register Aliases

Use `.set` to give a register a meaningful name for the duration of a file. Define aliases near the top of each `.asm` file after `.include "Common.s"`.

```asm
.set pPlayer, r3
.set score,   r4
.set temp,    r12
```

From that point, `pPlayer`, `score`, and `temp` can be used anywhere a register is expected. Aliases are per-file. You can rename an alias at any point.

### Common.s Macros

**`load reg, address`** — load a 32-bit address into a register (lis + ori):
```asm
load r3, 0x80123456    # r3 = 0x80123456
```

**`loadwz reg, address`** — load a 32-bit word from a memory address:
```asm
loadwz r3, 0x80123456  # r3 = *(int*)0x80123456
```

**`loadbz reg, address`** — load a byte from a memory address:
```asm
loadbz r3, 0x80123456  # r3 = *(byte*)0x80123456
```

**`branch reg, address`** — jump to an absolute address (no link):
```asm
branch r12, 0x80012345
```

**`branchl reg, address`** — call a function at an absolute address (with link):
```asm
branchl r12, 0x800F9BDC
```

**`backupall`** — save r3–r31 and LR to the stack (0x100 byte frame):
```asm
backupall
```

**`restoreall`** — restore r3–r31 and LR from the stack:
```asm
restoreall
```

### ASM Template

```asm
.include "Common.s"

# Author:      YourName
# Address:     0x80XXXXXX
# Instruction: nop
# *This is a note

.set pPlayer, r3
.set score,   r4

    backupall
    lwz  score, 0x44(pPlayer)
    addi score, score, 1
    stw  score, 0x44(pPlayer)
    restoreall
```

---

## C Files

### Entry Function

The entry function is whatever function the gecko handler will branch to at the memory address specified in teh `Address` comment line.

The entry function must match the filename — for `myCode.c`, define `void myCode()`. The script:
- Moves the entry function to the top of the compiled output so it runs first.
- Injects BACKUP (saves r3–r31, LR, and any used FPRs) before it runs.
- Injects RESTORE (reloads all saved registers) after it returns.
- Replaces all `blr` instructions with forward branches to the terminator.

The entry function can't return a value since the automatic stack frame manageament will overwrite the returned value. Therefore just make all entry functions `void`.

Helper functions (any name other than the entry function) are normal C.

### Register Management

The script will automatically save all registers in codes written in C. This allows you to freely write code without worrying about preserving the game state. However, this also adds the limitation that you cannot modify registers in your code if you'd like to. In that case, you would need to write your mod in ASM.

Use `REG()` to set an alias to a register value at the injection point. Read-only: Writes to REG variables will work within the gecko code, but will not persist afterwards since RESTORE reloads the original saved register state.

```c
void myCode() {
    REG(int, strikes, 3);    // at the injection point, r3 is the strike count
    if (strikes == 3) {
        strikes = 0          // r3 set to 0 in the gecko code
    }
}
                             // after gecko code completes, r3 returns to original value
```

> VSCode may show a squiggle on `REG()` syntax — ignore it, GCC compiles it correctly.

### Memory Access

```c
// Read/write a single value
value_atAddr(int,   0x80123456) = 10;
int x = value_atAddr(int, 0x80123456);
value_atAddr(float, 0x8012ABCD) = FLOAT(3, 2);

// Read/write an array element in game memory
value_atAddr(int, 4, 0x80AABBCC)[2] = 0xFF;

// Named pointer (use * to dereference)
DECLARE_VARIABLE(int, score, 0x80123456);
*score = 10;
```

### Game Function Calls

```c
// Named macro (preferred for readability)
#define PlaySound function_atAddr(void, 0x800c836C, int, int, int, int)
PlaySound(soundID, 127, 0x3f, 0x0);

// Inline one-off call
function_atAddr(void, 0x800c836C, int, int, int, int)(soundID, 127, 0x3f, 0x0);
```

### Floats and Arrays — Important Limitation

The gecko payload is loaded at an unknown address at runtime. Any data that would normally live in `.rodata` or `.data` (static arrays, float literals, string constants) requires absolute memory addresses that are invalid in this context. **The script will error if `.rodata` or `.data` sections are generated.** Use stack-based alternatives instead. Here are the workarounds:

**Floats:**

Do not use float literals directly — they go into `.rodata`:
```c
float x = 1.5f;           // ❌ generates .rodata — will error
static float x = 1.5f;   // ❌ generates .rodata — will error
```

Use `FLOAT(num, den)` for rational values (computed at runtime from integers):
```c
float speed   = FLOAT(3, 2);     // ✅ 1.5f
float quarter = FLOAT(1, 4);     // ✅ 0.25f
float ten     = FLOAT(10, 1);    // ✅ 10.0f
float pi      = FLOAT(355, 113); // ✅ ~3.14159f
```

Use `FLOAT_BITS(hex)` for arbitrary values (stores bit pattern as int on stack):
```c
float x = FLOAT_BITS(0x3FC00000);  // ✅ 1.5f
float y = FLOAT_BITS(0xBF800000);  // ✅ -1.0f
```

Use an [IEEE 754 converter](https://www.h-schmidt.net/FloatConverter/IEEE754.html) to find the hex for any value.

To read a float from game memory, use `value_atAddr`:
```c
float gameSpeed = value_atAddr(float, 0x80123456);  // ✅ read from game memory
```

**Arrays:**

Do not use static or global arrays — they go into `.rodata`/`.data`:
```c
static int arr[] = {1, 2, 3};   // ❌ generates .rodata — will error
```

Declare arrays on the stack instead:
```c
int arr[3] = {1, 2, 3};         // ✅ lives on the stack
int len = LEN(arr);              // ✅ = 3
int val = arr[1];                // ✅ = 2
```

To access an array in game memory, use `value_atAddr`:
```c
int gameArr = value_atAddr(int, 3, 0x80ABCDEF)[1];  // ✅ read from game memory
```

### C Template

```c
#include "Common.h"

// Author:      YourName
// Address:     0x80XXXXXX
// Instruction: nop
// *This is a note

// Game memory
#define gScore  value_atAddr(int,   0x80100000)
#define gLives  value_atAddr(int,   0x80100004)
#define gFlags  value_atAddr(byte,  0x8010000C)

// Game functions (types only — no parameter names)
#define PlaySound function_atAddr(void, 0x800c836C, int, int, int, int)

// Helpers
static int clamp(int val, int min, int max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

// Entry function — name matches filename
void myCode() {
    REG(int, currentScore, 20);     // r20 at injection point

    float mult  = FLOAT(3, 2);      // 1.5f — stack float
    int bonus[] = {10, 25, 50};     // stack array

    gScore = clamp(currentScore + bonus[1], 0, 99999);
    gFlags = 1;
    PlaySound(0x1, 127, 0x3f, 0x0);
}
```

---

## Debug Artifacts (`-d`)

| File | Description |
|---|---|
| `.rewritten.c` | C source after entry function reordering |
| `.o` | Compiled object file |
| `.elf` | Linked ELF at base address 0x0 |
| `.ld` | Linker script used |
| `.disasm.txt` | objdump disassembly |

---

## Dolphin Integration

The script reads and writes the Dolphin GameSettings ini directly:
- New codes are appended to `[Gecko]` and added to `[Gecko_Enabled]`.
- Existing codes (matched by name) are updated in place.
- All other ini content is preserved exactly.
- A blank or missing ini is initialized with the correct structure automatically.

If provided in `config.json`, the script will also automatically launch Dolphin upon successfully writing the code.
