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

On **Windows**, the graphical installer handles everything including devkitPPC — no further steps needed.

On **macOS/Linux**, install the PowerPC toolchain after installing devkitpro-pacman:
```
dkp-pacman -S devkitPPC
```

The installer sets a `DEVKITPPC` environment variable automatically. The script reads this to locate the tools — no manual path configuration needed. If the variable is missing or incorrect, set it manually to your devkitPPC install path.

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

To configure the repo for your environment, copy `config.template.json` to `config.json` and fill in the fields.

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

## Dolphin Integration

The script reads and writes the Dolphin GameSettings ini directly:
- New codes are appended to `[Gecko]` and added to `[Gecko_Enabled]`.
- Existing codes (matched by name) are updated in place.
- All other ini content is preserved exactly.
- A blank or missing ini is initialized with the correct structure automatically.

If provided in `config.json`, the script will also automatically launch Dolphin upon successfully writing the code.

---

## Source File Comment System

To specify features of gecko codes (author, injection address, etc) we use comment lines. Both `.c` and `.asm` files use the same comment syntax. C files use `//`, ASM files use `#`.

| Comment | Required | Description |
|---|---|---|
| `// Address: 0x80XXXXXX` | Yes | Injection address |
| `// Author: YourName` | No | Author for gecko header |
| `// Instruction: <asm>` | No | PPC instruction appended after RESTORE (re-executes the overwritten instruction) |
| `// *Note text` | No | Note line appended after code block. Repeatable. |
| `// State: menu\|game\|4\|5` | No | Enables conditional wrapper for MSSB game state* |

The code name is derived automatically from the source filename (e.g. `myCode.c` → `myCode`). The C entry function name replaces spaces with underscores (e.g. `My Code.c` -> `my_code()`).

**`// Instruction`** takes a PPC assembly instruction (e.g. `li r3, 0`) and appends it to the very end of the payload, just before the terminator. Use this when the gecko code's branch overwrites an instruction that still needs to execute.

*MSSB dynamically changes the RAM during runtime by loading `rel` files. We use conditional wrappers to only inject codes at certain game states. For non-MSSB games, this can be ignored.

| State | Executes when... |
|---|---|
| `menu` or `4` | In the menu (`280E877C 00000004`) |
| `game` or `5` | In-game (`280E877C 00000005`) |

### Gecko Output Format

```
$Name [Author]
280E877C 00000004   <- conditional wrapper (only if // State present)
C2XXXXXX LLLLLLLL   <- C3 if address >= 0x81000000
...BACKUP...
...code...
...RESTORE...
IIIIIIII XXXXXXXX   <- overwritten instruction (only if // Instruction present)
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

---

## ASM Files

Include `Common.s` at the top of every `.asm` file for access to the standard macros.

- Assembled with `powerpc-eabi-as` (`-mregnames -mbig`).
- No automatic BACKUP/RESTORE injection. Manage the stack yourself using the macros in `Common.s`.
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

The entry function is whatever function the gecko handler will branch to at the memory address specified in the `Address` comment line.

The entry function must match the filename — for `myCode.c`, define `void myCode()`. The script:
- Injects BACKUP (saves r3–r31, LR, and any used FPRs) before it runs.
- Injects RESTORE (reloads all saved registers) after it returns.
- Replaces all `blr` instructions with forward branches to the terminator.

The entry function can't return a value since the automatic stack frame manageament will overwrite the returned value. Therefore just make all entry functions `void`.

Helper functions (any name other than the entry function) are normal C.

### Register Access
 
Some mods rely on specific registers from the game state of the injection point. Since C codes automatically backup/restore r3-r31, we access them through the saved stack.
*Note: these macros will only work as intended in the entry function, not any helpers*

- `READ_GAME_REG(type, name, reg_num)` — reads a register value from the stack slot. r3–r31 only.
- `WRITE_GAME_REG(reg_num, val)` — writes a register value to the stack slot. r3–r31 only.
- `READ_REG(type, name, num)` — live register read. Primarily for r0–r2 which have no stack slot. Use only if needed at beginning of entry functions. Use cautiously.
 
```c
void myCode() {
    READ_GAME_REG(int, score, 3);   // r3 = score at injection point
    if (score < 10) {
        WRITE_GAME_REG(3, 10);      // sets r3 to 10 after gecko code completes and stack is restored
    }
}
```

### Memory Access

```c
// Read/write a single value
VAR_ADDRESS(int,   0x80123456) = 10;
int x = VAR_ADDRESS(int, 0x80123456);
float f = VAR_ADDRESS(float, 0x8012ABCD);

// Read/write an array element in game memory
VAR_ADDRESS(int, 4, 0x80AABBCC)[2] = 0xFF;
```

### Game Function Calls

```c
// Named macro (preferred for readability)
#define PlaySound FUNCTION_ADDRESS(void, 0x800c836C, int, int, int, int)
PlaySound(soundID, 127, 0x3f, 0x0);

// Inline one-off call
FUNCTION_ADDRESS(void, 0x800c836C, int, int, int, int)(soundID, 127, 0x3f, 0x0);
```

### Floats and Arrays — Important Limitation
 
The gecko payload is loaded at an unknown address at runtime. Any data that would normally live in `.rodata` or `.data` (static arrays, float literals, string constants) requires absolute memory addresses that are invalid in this context. **The script will error if `.rodata` or `.data` sections are generated.** Use stack-based alternatives instead.

*NOTE: A future solution would involve the user specifying the memory address to palce `.rodata`. This feature does not yet exist and the limitations currently have to be dealt with. This may come in the future.*
 
**Floats:**
 
Do not declare float constants of any kind. Float literals generate `.rodata`, which uses absolute addresses invalid at an unknown payload address. The build will error.
 
```c
float x = 1.5f;         // ❌ generates .rodata — will error
static float x = 1.5f; // ❌ generates .rodata — will error
```
 
Read floats directly from game memory instead:
```c
float gameSpeed = VAR_ADDRESS(float, 0x80123456);   // ✅ read float from game memory
```
 
Float arithmetic between game-memory values works correctly — both values load via `lfs` into FPRs and the result stores via `stfs`:
```c
gSpeed = gSpeed * gSpeed2;   // ✅ game memory floats only
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
 
To access an array in game memory, use `VAR_ADDRESS`:
```c
int gameArr = VAR_ADDRESS(int, 3, 0x80ABCDEF)[1];  // ✅ read from game memory
```
 
 
### Strings
 
String literals (`"Hello"` or `const char*`) go into `.rodata` and will cause a build error. Use a char array initializer list instead — each element is an integer constant that stays on the stack:
 
```c
char msg[] = {'H','e','l','l','o','!','\0'};   // ✅ stack only
OSReport(msg);
```
 
For longer strings, `STR4`/`STR2`/`STR1` pack multiple characters into a single store each:
 
```c
char buf[8];
STR4(buf+0, 'H','e','l','l');   // 4 chars in one store
STR2(buf+4, 'o','!');           // 2 chars in one store
STR1(buf+6, '\0');              // null terminator
OSReport(buf);
```
 
The buffer must be large enough for the string plus a null terminator.

### C Template

```c
#include "Common.h"

// Author:      YourName
// Address:     0x80XXXXXX
// Instruction: nop
// *This is a note

// Game memory
#define gScore  VAR_ADDRESS(int,   0x80100000)
#define gLives  VAR_ADDRESS(int,   0x80100004)
#define gFlags  VAR_ADDRESS(byte,  0x8010000C)

// Game functions (types only — no parameter names)
#define PlaySound FUNCTION_ADDRESS(void, 0x800c836C, int, int, int, int)

// Helpers
static int clamp(int val, int min, int max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

// Entry function — name matches filename
void myCode() {
    READ_GAME_REG(int, currentScore, 20);   // r20 at injection point

    int bonus[] = {10, 25, 50};             // stack array

    int newScore = clamp(currentScore + bonus[1], 0, 99999);
    gScore = newScore;
    WRITE_GAME_REG(20, newScore);           // game sees updated r20 after gecko code returns
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
