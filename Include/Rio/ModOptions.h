/*###########################################################
# ModOptions.h -- the mod toggle bytes
###########################################################*/
// Author: LittleCoaks
//
// One byte per user-toggleable mod option, in a fixed block of claimed
// free memory. "Gecko Codes/Menu/Options Menu.c" is the UI that flips
// them; any other code can read one with:
//
//     #include "Include/Rio/ModOptions.h"
//
//     if (ModOptionOn(MODOPT_CPU_SPRINT))
//         ...                       // the mod's body, gated on the toggle
//
// Nothing here reaches into the game -- the bytes are just flags in RAM.
// A mod becomes toggleable by reading its flag, which is a one-line change
// at the top of that mod's per-frame code. None of the mods in this repo do
// that yet; the ids below name what the menu rows are FOR, so the UI and the
// mods agree on which byte is which the moment a mod opts in.
//
// The block is NOT saved anywhere: it is zeroed once per console session (the
// first time the Options screen runs) and every option starts OFF. Persisting
// it to the memory card is a later stage.

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
#define MODOPT_COUNT        5

#define MODOPT_ADDR(id)    (MODOPT_BASE + (id) * 4)
#define ModOptionValue(id) VAR_ADDRESS(u32, MODOPT_ADDR(id))
#define ModOptionOn(id)    (ModOptionValue(id) != 0)

/* Zero the whole block. The pointer is volatile on purpose: a plain zeroing
 * loop is exactly the shape GCC rewrites into a call to memset, and a gecko
 * payload has no libc to call. */
static void ModOptions_Reset(void)
{
    volatile u32* opt = (volatile u32*)MODOPT_BASE;
    int i;

    for (i = 0; i < MODOPT_MAX; i++)
        opt[i] = 0;
}

#endif /* MODOPTIONS_H */
