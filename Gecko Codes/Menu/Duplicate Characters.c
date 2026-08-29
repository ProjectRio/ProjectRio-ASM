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
// This one is a TOGGLE, so it has to be able to put the game back: it captures
// each site's original word the first time it sees an unpatched value and
// restores it when the option is switched off. Capturing at runtime also means
// the menus.rel sites need no build-time knowledge of the REL's contents, and
// it re-arms by itself every time menus.rel reloads.
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

#define DCFlushRange      ((void (*)(u32, u32))0x8006E894)
#define ICInvalidateRange ((void (*)(u32, u32))0x8006E94C)

// (site, patched word) pairs. The DOL sites are always valid; the menus.rel
// ones are only touched while rel == 4, since menus.rel and game.rel share one
// arena slot and writing the wrong one corrupts the other module.
typedef struct { u32 addr; u32 patched; } CodePatch;

static const CodePatch DOL_PATCHES[] = {
    { 0x80067BAC, NOP },            // stb r5, 0x4757(r3)  x5
    { 0x80067BC8, NOP },
    { 0x80067BE4, NOP },
    { 0x80067C00, NOP },
    { 0x80067C1C, NOP },
    { 0x8004E548, NOP },            // sth r0, 0x18(r8)
    { 0x8004E6B0, CMPWI_R0_FF },    // cmpw r0, r7  ->  cmpwi r0, 0xFF
};
static const CodePatch REL_PATCHES[] = {
    { 0x8064EC28, NOP },
    { 0x8064EC38, NOP },
    { 0x8064ECE8, NOP },
    { 0x806553F8, NOP },
    { 0x806553D4, CMPWI_R0_FF },
};
#define N_DOL ((int)(sizeof(DOL_PATCHES) / sizeof(DOL_PATCHES[0])))
#define N_REL ((int)(sizeof(REL_PATCHES) / sizeof(REL_PATCHES[0])))

// Saved originals, in claimed RAM -- see ClaimedFreeMemory.h. 0 = not captured
// yet. No payload statics: this is a per-frame code and the helpers below touch
// them, which is exactly the case that faulted through r31 in earlier mods.
#define g_saved(i) VAR_ADDRESS(u32, 0x802EB060 + (i) * 4)

/* Apply or undo one site, capturing its original the first time we see an
 * unpatched value. Writing instruction memory needs the dcache flushed and the
 * icache line invalidated, or the CPU keeps running the stale instruction. */
static void PatchSite(int slot, u32 addr, u32 patched, bool on)
{
    u32 cur = VAR_ADDRESS(u32, addr);

    if (on)
    {
        if (cur == patched)
            return;                      /* already applied */
        g_saved(slot) = cur;             /* capture, then patch */
        VAR_ADDRESS(u32, addr) = patched;
    }
    else
    {
        if (cur != patched || g_saved(slot) == 0)
            return;                      /* not ours, or nothing captured */
        VAR_ADDRESS(u32, addr) = g_saved(slot);
    }
    DCFlushRange(addr & ~31, 64);
    ICInvalidateRange(addr & ~31, 64);
}

CGECKO(DuplicateCharacters, .state = MSSB_MENU);
void DuplicateCharacters()
{
    bool on = ModOptionOn(MODOPT_DUPLICATES);
    int i;

    for (i = 0; i < N_DOL; i++)
        PatchSite(i, DOL_PATCHES[i].addr, DOL_PATCHES[i].patched, on);
    for (i = 0; i < N_REL; i++)
        PatchSite(N_DOL + i, REL_PATCHES[i].addr, REL_PATCHES[i].patched, on);

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
