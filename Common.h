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
 * value_atAddr(type, addr)                  — single value
 * value_atAddr(type, count, addr)           — 1D array
 * value_atAddr(type, rows, cols, addr)      — 2D array
 *
 * Examples:
 *   value_atAddr(int,  0x80123456) = 10;
 *   int x = value_atAddr(int, 0x80123456);
 *   value_atAddr(byte, 4, 0x80AABBCC)[2] = 0xFF;
 */
#define GET_MACRO(_1,_2,_3,_4,NAME,...) NAME
#define value_atAddr(...) GET_MACRO(__VA_ARGS__, arrayValue2D_atAddr, arrayValue_atAddr, singleValue_atAddr)(__VA_ARGS__)

#define singleValue_atAddr(type, addr)                    (*(type *)(addr))
#define arrayValue_atAddr(type, count, addr)              (*(type (*)[count])(addr))
#define arrayValue2D_atAddr(type, rows, cols, addr)       (*(type (*)[rows][cols])(addr))

/* ── Function calls ──────────────────────────────────────────────────────── */
/*
 * Call a game function by its memory address.
 * Expands to an inline cast — no declaration, no scope issues, works anywhere.
 *
 * Examples:
 *   function_atAddr(void, 0x800c836C, int, int, int, int)(soundID, 127, 0x3f, 0x0);
 *   function_atAddr(int,  0x80123456, float)(1.5f);
 *
 * For repeated calls, assign to a local function pointer:
 *   void (*PlaySound)(int, int, int, int) = (void(*)(int,int,int,int))(0x800c836C);
 *   PlaySound(soundID, 127, 0x3f, 0x0);
 *   PlaySound(soundID2, 64, 0x3f, 0x0);
 */
#define function_atAddr(returnType, addr, ...) ((returnType (*)(__VA_ARGS__))(addr))

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
 * directly to memory via value_atAddr or use inline asm.
 *
 * Note: VSCode may show a squiggle on this syntax — ignore it, GCC handles it correctly.
 */
#define REG(type, name, num) register type name __asm__(#num)

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
 *   via value_atAddr, or use __asm__ directly.
 *
 * FLOATS:
 *   Declare float constants as static to ensure correct PC-relative addressing:
 *     static const float MY_CONST = 1.5f;  // safe
 *   For zero memory overhead, use an integer bit-cast:
 *     union { unsigned int i; float f; } u = {0x3FC00000};  // 1.5f
 */

#endif /* COMMON_H */