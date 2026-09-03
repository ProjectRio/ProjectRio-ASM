/*###########################################################
# ModOptions.h -- the mod toggle bytes
###########################################################*/
// Author: LittleCoaks
//
// One word per user-toggleable mod option, in a fixed block of claimed
// free memory. "RioModPack/Options Menu.c" is the UI that flips them.
//
// THE MODS THEMSELVES NEVER INCLUDE THIS FILE. A mod in "Gecko Codes/" is
// written as if the pack did not exist, and RioModPack.c wires the toggle in
// from OUTSIDE, around the #include, using CGecko's two hooks:
//
//     CGECKO_GATE_ADDR  = MODOPT_ADDR(id)   the code is not RUN while off
//                                           (fire-and-forget mods)
//     CGECKO_ACTIVE     = ModOptionOn(id)   the code runs and restores
//                                           itself while off (mods that
//                                           patch game code or state)
//
// Standalone, a mod's CGECKO_ACTIVE is the constant 1 and its gecko build
// carries no trace of the pack. Only pack-native code under RioModPack/
// (the Options UI, Gecko Codes, Custom Music) reads these words directly.
//
// The block is NOT saved anywhere: it is reset once per console session (the
// first time the Options screen runs) to the defaults in
// ModOptions_ApplyDefaults. Persisting it to the memory card is a later stage.

#ifndef MODOPTIONS_H
#define MODOPTIONS_H

#include "CGecko/Common.h"
#include "Include/types.h"   // u8/u32 -- so this header stands alone

/* 16 WORDS of claimed free memory -- see ClaimedFreeMemory.h. Sized well past
 * MODOPT_COUNT so new options never move the block (an option's id IS its
 * offset, so moving the base or renumbering an id would silently point a
 * half-rebuilt mod at the wrong flag).
 *
 * WHY WORDS AND NOT BYTES: a gecko conditional compares either 32 bits (`20`)
 * or 16 bits with a mask (`28`). One option per WORD means a gate is just
 *     202EB010 00000001      // if the word at 0x802EB010 == 1
 * with no mask or shift arithmetic, which is what cgecko's CGECKO_GATE_ADDR
 * emits and what you would hand-write to gate a raw .ini code. Packed bytes
 * would need a different mask per option depending on its alignment. */
#define MODOPT_BASE 0x802EB010
#define MODOPT_MAX  16

/* Option ids. APPEND ONLY, and keep MODOPT_COUNT last. */
#define MODOPT_WIDESCREEN   0
#define MODOPT_CPU_SPRINT   1
#define MODOPT_INSTANT_RNG  2
#define MODOPT_DUPLICATES   3
#define MODOPT_SUPERSTARS   4
#define MODOPT_MUSIC        5
#define MODOPT_GECKO        6
#define MODOPT_NIGHT_MARIO  7
#define MODOPT_COUNT        8

#define MODOPT_ADDR(id)    (MODOPT_BASE + (id) * 4)
#define ModOptionValue(id) VAR_ADDRESS(u32, MODOPT_ADDR(id))
#define ModOptionOn(id)    (ModOptionValue(id) != 0)

/* Options that are ON unless the user turns them off.
 *
 * Every option starts at 0 and 0 means OFF, which is right for a mod: nothing
 * changes until you ask for it. MODOPT_GECKO is the exception -- it does not
 * switch a mod on, it switches OFF the emulator's gecko code handler, so 0 has
 * to mean "leave the handler alone" and that is the ON state on the menu.
 *
 * A zeroed block is therefore indistinguishable from "user turned gecko codes
 * off", and the code that enforces the toggle runs from boot -- long before
 * the Options screen has ever run and reset anything. So the block carries a
 * sentinel in its LAST word: seeing it means the defaults below have been
 * applied at least once. ModOptions_Reset clears it along with everything
 * else, so a reset re-seeds rather than leaving the block half-initialised.
 *
 * The sentinel costs the top word of the block, so options may run 0..14. */
#define MODOPT_SENTINEL_IDX (MODOPT_MAX - 1)
#define MODOPT_SENTINEL     0x4F505431   /* 'OPT1' -- bump if defaults change */

static void ModOptions_ApplyDefaults(void)
{
    volatile u32* opt = (volatile u32*)MODOPT_BASE;

    if (opt[MODOPT_SENTINEL_IDX] == MODOPT_SENTINEL)
        return;

    opt[MODOPT_SENTINEL_IDX] = MODOPT_SENTINEL;
    opt[MODOPT_GECKO]        = 1;   /* extra gecko codes run unless turned off */
}

/* Zero the whole block, then put the default-ON options back. The pointer is
 * volatile on purpose: a plain zeroing loop is exactly the shape GCC rewrites
 * into a call to memset, and a gecko payload has no libc to call. */
static void ModOptions_Reset(void)
{
    volatile u32* opt = (volatile u32*)MODOPT_BASE;
    int i;

    for (i = 0; i < MODOPT_MAX; i++)
        opt[i] = 0;

    ModOptions_ApplyDefaults();
}

#endif /* MODOPTIONS_H */
