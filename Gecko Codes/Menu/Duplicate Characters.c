/*###########################################################
# Duplicate Characters
###########################################################*/
// Author: LittleCoaks

// *Lets the same character be drafted onto both teams, as many times as you
// *want. Converted from the community gecko code "All Duplicate Characters".
//
// HOW THE GAME TRACKS IT. Character select keeps a 54-byte "already taken"
// table, one byte per character id, at Static_Stats_Tables + 0x4757
// (0x803530F7). A non-zero byte greys that character out. The stock code sets
// it from five `stb r5, 0x4757(r3)` sites in addRemoveCharVariantRelated
// (0x80067B40) plus their menus.rel counterparts.
//
// The mod is therefore: stop the game marking anyone taken, clear whatever is
// already marked, and then put the mark back for JUST the two captains -- they
// are genuinely unavailable, the rest are not.
//
// WHY THIS IS A PER-FRAME CODE AND NOT A PILE OF 04 WRITES. The original is a
// static patch list, which is fine for a code you enable in an ini and forget.
// This one is a TOGGLE, so it has to be able to put the game back -- and a REL
// patch has to be re-applied every time menus.rel reloads anyway.
//
// Original gecko code, for reference:
//     003530f7 00350000   zero 54 bytes at 0x803530F7   (0x35+1, the count is
//     003c6050 002300ff   36 bytes of 0xFF at 0x803C6050  the HIGH halfword --
//                                                         see codehandler.s
//                                                         `rlwinm r10,r4,16,16,31`)
//     04067bac/bc8/be4/c00/c1c 60000000   nop the five stb sites (DOL)
//     0464ec28/ec38/ece8       60000000   the same three, menus.rel
//     0404e548 60000000  046553f8 60000000
//     0404e6b0 2C0000FF  046553d4 2C0000FF
//     c264f394 ...       re-mark the two captains
#include "Include/game/UnknownHomes_Game.h"
#include "Include/Local/Legacy.h"
#include "RioModPack/ModOptions.h"

#define TAKEN_TABLE   0x803530F7    // Static_Stats_Tables + 0x4757, 54 bytes
#define TAKEN_COUNT   54            // 0x35 + 1, one byte per character id
#define CHARSEL_SLOTS 0x803C6050    // charSelectStruct + 0x28, 36 bytes
#define CHARSEL_COUNT 36            // 0x23 + 1
#define CAPTAIN_A     0x803C6726    // cursorPositions + 0x02
#define CAPTAIN_B     0x803C672F    // cursorPositions + 0x0B

#define NOP           0x60000000
#define CMPWI_R0_FF   0x2C0000FF    // cmpwi r0, 0xFF   (0xFF = "empty slot")

// (site, original, patched). Patching is Common.h's PatchInstruction_Conditional,
// which only writes when the expected word is there -- so ON and OFF are the same
// call with the last two arguments swapped, and both are idempotent. It also
// re-arms itself for free: after a menus.rel reload the original is back, the
// site matches again, and the next frame re-applies. No saved-state RAM needed.
//
// The REL originals are NOT the same instructions as their DOL counterparts
// (stwx vs sth, cmpw r3,r0 vs cmpw r0,r7) -- read out of live RAM with menus.rel
// resident, not assumed.
typedef struct { u32 addr; u32 orig; u32 patched; } CodePatch;

static const CodePatch PATCHES[] = {
    /* --- main.dol: addRemoveCharVariantRelated, five stb sites -------- */
    { 0x80067BAC, 0x98A34757, NOP },          /* stb r5, 0x4757(r3) */
    { 0x80067BC8, 0x98A34757, NOP },
    { 0x80067BE4, 0x98A34757, NOP },
    { 0x80067C00, 0x98A34757, NOP },
    { 0x80067C1C, 0x98A34757, NOP },
    { 0x8004E548, 0xB0080018, NOP },          /* sth r0, 0x18(r8) */
    { 0x8004E6B0, 0x7C003800, CMPWI_R0_FF },  /* cmpw r0, r7 */
    /* --- menus.rel: the same edits on the menu side ------------------- */
    { 0x8064EC28, 0x98E44757, NOP },          /* stb r7, 0x4757(r4) */
    { 0x8064EC38, 0x98E54757, NOP },          /* stb r7, 0x4757(r5) */
    { 0x8064ECE8, 0x98A34757, NOP },          /* stb r5, 0x4757(r3) */
    { 0x806553F8, 0x7C1EF92E, NOP },          /* stwx r0, r30, r31 */
    { 0x806553D4, 0x7C030000, CMPWI_R0_FF },  /* cmpw r3, r0 */
};
#define N_PATCHES ((int)(sizeof(PATCHES) / sizeof(PATCHES[0])))

CGECKO(DuplicateCharacters, .state = MSSB_MENU);
void DuplicateCharacters()
{
    bool on = ModOptionOn(MODOPT_DUPLICATES);
    int i;

    for (i = 0; i < N_PATCHES; i++)
    {
        if (on)
            PatchInstruction_Conditional(PATCHES[i].addr, PATCHES[i].orig,
                                         PATCHES[i].patched);
        else
            PatchInstruction_Conditional(PATCHES[i].addr, PATCHES[i].patched,
                                         PATCHES[i].orig);
    }

    if (!on)
        return;

    // Clear every "taken" mark, then put back only the two captains -- they
    // really are unavailable. This is the C2 the original code injected at
    // 0x8064F394, hoisted to per-frame: the table is only read while the
    // character-select screen is drawing, so refreshing it each frame is
    // equivalent and needs no second injection site.
    for (i = 0; i < TAKEN_COUNT; i++)
        VAR_ADDRESS(u8, TAKEN_TABLE + i) = 0;
    for (i = 0; i < CHARSEL_COUNT; i++)
        VAR_ADDRESS(u8, CHARSEL_SLOTS + i) = 0xFF;

    VAR_ADDRESS(u8, TAKEN_TABLE + VAR_ADDRESS(u8, CAPTAIN_A)) = 1;
    VAR_ADDRESS(u8, TAKEN_TABLE + VAR_ADDRESS(u8, CAPTAIN_B)) = 1;
}
