/*###########################################################
# Gecko Codes
###########################################################*/
// Author: LittleCoaks
//
// *Turns the emulator's gecko code handler on and off, so extra codes layered
// *on top of this build can be disabled without touching any of its own mods.
//
// WHAT THIS IS FOR. RioModPack ships as a DOL: its mods are branches baked
// into main.dol and a per-frame hook at 0x80009404, none of which involve the
// gecko code handler. A user can still enable ordinary gecko codes on top in
// Rio or Dolphin, and those go through the handler. This toggle switches that
// handler, and only that handler -- the pack's own mods are untouched either
// way, because they were never running through it.
//
// HOW THE HANDLER IS ENTERED, AND WHY THERE IS NOTHING TO NOP. A normal gecko
// setup patches a `bl codehandler` over some instruction in the game's frame
// loop, and disabling it is a matter of restoring that word. Dolphin does NOT
// do that. In Core/GeckoCode.cpp, RunCodeHandler builds a stack frame by hand
// and then simply sets
//
//     LR = HLE_TRAMPOLINE_ADDRESS;                 // 0x80002FFC
//     pc = npc = ENTRY_POINT;                      // 0x800018A8
//
// -- a "phantom branch-and-link" driven from the host side once per frame.
// There is no hook instruction anywhere in the game to put back.
//
// So the lever is the handler itself. It is ordinary PPC code sitting in MEM1
// at 0x80001800..0x80003000, and its entry point is INSTALLER_BASE + 0xA8 =
// 0x800018A8, which stock reads
//
//     9421FF54    stwu r1, -0xAC(r1)
//
// Writing a `blr` there makes the phantom call return immediately and no code
// in the list is ever walked. Returning is safe: HLE_Misc::GeckoReturnTrampoline
// restores r1, npc, LR, CR and FPR0-13 entirely from the frame the HOST wrote
// before the call, so it does not care whether the handler ran. The handler's
// own `stmw`/`lmw` of the GPRs never happens either, which is fine -- if it
// never saved them it never had to put them back.
//
// WHY NOT BLANK THE CODE LIST INSTEAD. The list base is recoverable at runtime
// (Rio writes it into the handler as a lis/ori pair at 0x80001904/0x80001908,
// so base = ((*0x80001904 - 0x3DE00000) << 16) | (*0x80001908 - 0x61EF0000)),
// and writing an F0 terminator at its head would also stop every code. But
// that means saving and restoring 8 bytes of somebody else's data and tracking
// a base that moves with the list; one conditional word at a fixed address is
// smaller and cannot get out of sync.
//
// SCOPE -- worth knowing before using it. This disables EVERY code in the
// handler's list, which under Project Rio includes Rio's own built-in codes,
// not just the ones a user added. The list is a flat run of code words with no
// record of where each came from, so there is nothing to tell them apart at
// runtime. Anything of Rio's that depends on its codes (stat tracking and the
// like) is off while this is off.
#include "Include/game/UnknownHomes_Game.h"
#include "RioModPack/ModOptions.h"

// Dolphin/Rio's Gecko constants (Source/Core/Core/GeckoCode.h).
#define GECKO_INSTALLER_BASE 0x80001800
#define GECKO_ENTRY          0x800018A8   // INSTALLER_BASE_ADDRESS + 0xA8

#define GECKO_ENTRY_STOCK    0x9421FF54   // stwu r1, -0xAC(r1)
#define GECKO_ENTRY_OFF      0x4E800020   // blr

// The handler writes MAGIC_GAMEID to its first word on install, and
// HLE_Misc::GeckoCodeHandlerICacheFlush increments it once per frame for the
// first five frames and then leaves it alone. So the word reads MAGIC..MAGIC+5
// whenever a handler is actually installed, and anything else means there is
// none -- no codes enabled, or a build running outside an emulator that
// installs one. Never write to 0x800018A8 in that case: with no handler there
// it is just some other code's memory.
#define GECKO_MAGIC       0xD01F1BAD
#define GECKO_MAGIC_SPAN  5

// Runs from boot, not just in menus: the handler is live during a match too,
// and a toggle that only took effect on the menu would be a lie.
CGECKO(GeckoCodes, .state = MSSB_ALWAYS,
       .notes = "Turns extra gecko codes on or off. This build's own mods are "
                "unaffected. Under Rio this also disables Rio's built-in codes.");
void GeckoCodes()
{
    u32 magic;

    // Cheap and idempotent; also covers the case where this code runs before
    // the Options screen has ever been opened.
    ModOptions_ApplyDefaults();

    magic = VAR_ADDRESS(u32, GECKO_INSTALLER_BASE) - GECKO_MAGIC;
    if (magic > GECKO_MAGIC_SPAN)
        return;                                  // no handler installed

    // Both directions are the same conditional write with the last two
    // arguments swapped, so each is idempotent and neither fights the other.
    // It also re-arms itself: Dolphin re-installs the handler whenever it
    // reloads the code list, which puts the stock word back, and the next
    // frame patches it again.
    if (ModOptionOn(MODOPT_GECKO))
        PatchInstruction_Conditional(GECKO_ENTRY, GECKO_ENTRY_OFF, GECKO_ENTRY_STOCK);
    else
        PatchInstruction_Conditional(GECKO_ENTRY, GECKO_ENTRY_STOCK, GECKO_ENTRY_OFF);
}
