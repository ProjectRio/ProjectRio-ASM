/*
 * Common.h
 * Helper macros and type definitions for C gecko code injection on GameCube.
 * See README.md for full documentation. For ASM macros, see Common.s.
 */

#ifndef COMMON_H
#define COMMON_H

/* ── Type aliases ────────────────────────────────────────────────────────── */

#ifndef __cplusplus // avoid false positive errors in IDEs
typedef unsigned char bool;
#define false 0
#define true  1
#endif

typedef unsigned char  byte;
typedef unsigned short halfword;
typedef unsigned int   word;

/* ── Memory access ───────────────────────────────────────────────────────── */
/*
 * VAR_ADDRESS(type, addr)                  — single value
 * VAR_ADDRESS(type, count, addr)           — 1D array
 * VAR_ADDRESS(type, rows, cols, addr)      — 2D array
 *
 * Examples:
 *   VAR_ADDRESS(int,  0x80123456) = 10;
 *   int x = VAR_ADDRESS(int, 0x80123456);
 *   VAR_ADDRESS(byte, 4, 0x80AABBCC)[2] = 0xFF;
 */
#define GET_MACRO(_1,_2,_3,_4,NAME,...) NAME
#define VAR_ADDRESS(...) GET_MACRO(__VA_ARGS__, arrayValue2D_atAddr, arrayValue_atAddr, singleValue_atAddr)(__VA_ARGS__)

#define singleValue_atAddr(type, addr)                    (*(type *)(addr))
#define arrayValue_atAddr(type, count, addr)              (*(type (*)[count])(addr))
#define arrayValue2D_atAddr(type, rows, cols, addr)       (*(type (*)[rows][cols])(addr))

/* ── Function calls ──────────────────────────────────────────────────────── */
/*
 * Call a game function by its memory address.
 * Expands to an inline cast — no declaration, no scope issues, works anywhere.
 *
 * Examples:
 *   FUNCTION_ADDRESS(void, 0x800c836C, int, int, int, int)(soundID, 127, 0x3f, 0x0);
 *   FUNCTION_ADDRESS(int,  0x80123456, float)(1.5f);
 *
 * For repeated calls, assign to a local function pointer:
 *   void (*PlaySound)(int, int, int, int) = (void(*)(int,int,int,int))(0x800c836C);
 *   PlaySound(soundID, 127, 0x3f, 0x0);
 *   PlaySound(soundID2, 64, 0x3f, 0x0);
 */
#define FUNCTION_ADDRESS(returnType, addr, ...) ((returnType (*)(__VA_ARGS__))(addr))

/* ── Register aliases ───────────────────────────────────────────────────────── */
/*
 * REG(type, name, num)
 *
 * Bind a C variable directly to a hardware register for the life of the function.
 * Reads and writes to 'name' go directly to that register.
 * Use with cgecko.py's naked entry function to access game registers at the
 * injection point. BACKUP saves them, your code modifies them, RESTORE reloads
 * them — the game sees the changes.
 *
 * Examples:
 *   REG(int,   score, 20);    // score IS r20
 *   REG(int,   lives, 21);    // lives IS r21
 *   REG(float, speed, 1);     // speed IS f1
 *
 * REG() is intended for reading game register values at the injection point.
 * Writes to REG() variables are NOT guaranteed to persist after RESTORE
 * reloads the saved register state. To modify game registers, write
 * directly to memory via VAR_ADDRESS or use inline asm.
 *
 * Note: VSCode may show a squiggle on this syntax — ignore it, GCC handles it correctly.
 */
#define READ_REG(type, name, num) register type name __asm__(#num)

/* ── Stack data helpers ─────────────────────────────────────────────────────── */
/*
 * FLOAT_BITS(hex)
 *
 * Create a float from its IEEE 754 hex bit pattern, stored on the stack.
 * Avoids .rodata entirely — no absolute address relocations generated.
 * Use this instead of float literals like 1.5f which would require static storage.
 *
 * Common values:
 *   0x3F800000  =  1.0f       0x40000000  =  2.0f
 *   0x3FC00000  =  1.5f       0xBF800000  = -1.0f
 *   0x41200000  = 10.0f       0x42C80000  = 100.0f
 *
 * Examples:
 *   float speed   = FLOAT_BITS(0x3FC00000);  // 1.5f
 *   float gravity = FLOAT_BITS(0x40000000);  // 2.0f
 */
#define FLOAT_BITS(hex) (*(float*)&(unsigned int){hex})

/*
 * FLOAT(num, den)
 *
 * Create a float from a rational number (numerator / denominator).
 * Computed at runtime from integers — no .rodata, no relocations.
 * Preferred over FLOAT_BITS when the value can be expressed as a fraction.
 *
 * Examples:
 *   float speed   = FLOAT(3, 2);    // 1.5f
 *   float quarter = FLOAT(1, 4);    // 0.25f
 *   float ten     = FLOAT(10, 1);   // 10.0f
 *   float pi      = FLOAT(355, 113); // ~3.14159f
 */
#define FLOAT(num, den) ((float)(num) / (float)(den))

/*
 * Stack arrays — declare and initialize normally:
 *   int  arr[4] = {10, 20, 30, 40};   // fine — lives on the stack
 *   arr[2] = arr[0] + arr[1];         // fine
 *
 * Do NOT use static or global arrays — those go into .rodata/.data and
 * generate absolute address relocations that break in a gecko payload.
 * The BACKUP frame provides 0x90 bytes of stack space (minus what GCC uses),
 * sufficient for small arrays.
 */

/* ── String helpers ─────────────────────────────────────────────────────────── */
/*
 * Strings cannot be declared as literals (const char* or char[] = "...") in
 * gecko injection code — they generate .rodata which uses absolute addresses
 * invalid at an unknown payload address.
 *
 * Use a char array initializer list instead — each element is an integer
 * constant and stays on the stack:
 *
 *   char msg[] = {'H','e','l','l','o','!','\0'};   // ✅ stack only
 *   OSReport(msg);
 *
 * For longer strings, STR4/STR2/STR1 pack multiple chars per store:
 *
 *   char buf[8];
 *   STR4(buf+0, 'H','e','l','l');  // stores 4 chars at buf+0
 *   STR2(buf+4, 'o','!');          // stores 2 chars at buf+4
 *   STR1(buf+6, '\0');             // stores null terminator
 *   OSReport(buf);
 */
#define STR4(buf, a, b, c, d) (*(int  *)(buf) = ((a)<<24)|((b)<<16)|((c)<<8)|(d))
#define STR2(buf, a, b)       (*(short*)(buf) = ((a)<< 8)|(b))
#define STR1(buf, a)          (*(char *)(buf) = (a))

/* ── Utilities ───────────────────────────────────────────────────────────── */

#define LEN(a)          (sizeof(a) / sizeof(*a))    // number of elements in array
#define SQUARE(a)       ((a) * (a))
#define offsetof(st, m) ((size_t)&(((st *)0)->m))   // byte offset of struct member

/* ── Tips ────────────────────────────────────────────────────────────────── */
/*
 * INJECTION SITES:
 *   Choose safe, idle instructions like li or lis as injection points.
 *   Avoid injecting mid-calculation or mid-loop.
 *
 * REGISTER VARIABLES:
 *   To read a game register value at the injection point, use REG:
 *     REG(int, score, 30);  // score reads from r30
 *     int newScore = score + 1;
 *   Note: writes to REG() variables do not persist after RESTORE.
 *   To modify a register the game will see, write to its memory address
 *   via VAR_ADDRESS, or use __asm__ directly.
 *
 * FLOATS:
 *   Declare float constants as static to ensure correct PC-relative addressing:
 *     static const float MY_CONST = 1.5f;  // safe
 *   For zero memory overhead, use an integer bit-cast:
 *     union { unsigned int i; float f; } u = {0x3FC00000};  // 1.5f
 */

#endif /* COMMON_H */