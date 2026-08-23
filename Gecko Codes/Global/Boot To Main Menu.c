/*###########################################################
# Boot To Main Menu
###########################################################*/
// Author: LittleCoaks
// *Boots directly to the main menu, skipping the intro + title screen.
// *Because the title screen is skipped, its slot-A memory-card check never
//  runs, so this code also drives that load itself:
//    - loads the Mario Baseball save data from memory card slot A
//    - no memory card            -> silently ignore and continue
//    - card but no MSSB data     -> silently create MSSB data
//    - not enough space          -> silently ignore and continue
//
// Two ways to drive the load, selected by CARD_LOAD_METHOD below:
//   GAME_WIZARD -- just fire the game's own card-check (0x8003F23C). This is
//                  the proven loader, but it is a DIALOG-DRIVEN WIZARD: it puts
//                  a text box on screen ("loading from Slot A", and yes/no
//                  prompts when there is no card or no MSSB data). Kept for
//                  reference / fallback.
//   SILENT      -- do the same load with no box: probe slot A first (SDK
//                  CARDProbeEx) and only invoke the wizard when a card is
//                  actually present (so a missing card never opens a prompt or
//                  hangs), then hide the wizard's dialog and auto-confirm its
//                  prompts for the load window.
#include "Include/Local/Legacy.h"

// TODO(decomp-migration): Menu data came from the Ghidra-exported MenuData.h.
// Replace with the decomp headers under Include/game/ and remap the
// symbol names -- see Include/_sync_report.txt and the decomp's
// docs/ghidra_name_conflicts.md.

/* ---- pick the load method ------------------------------------------------ */
#define CARD_LOAD_GAME_WIZARD 0   // fire 0x8003F23C, game shows its box/prompts
#define CARD_LOAD_SILENT      1   // probe-gate + hide the box + auto-confirm

#define CARD_LOAD_METHOD  CARD_LOAD_SILENT

/* =========================================================================
   Part 1 -- Boot straight to the main menu (the original gecko, in C).

   The boot sequence is a per-frame task (0x8063F904+) that walks the boot
   screens through a jump table at 0x806D7118: substate 7 -> screen 1 (the
   title screen, which performs the memory-card check) and substate 9 ->
   screen 5 (the main menu). Forcing the substate-9 dispatch instruction to
   `li r3, 5` makes the walk jump straight to the main menu, exactly like the
   original code:
       280E877C 00000000   if half[inningSetting.rel] == 0
       0463F964 38600005      [0x8063F964] = li r3, 5
       E2000001            endif
   ========================================================================= */
#define BOOT_TO_MENU_SITE   0x8063F964
#define LI_R3_5             0x38600005      // li r3, 5  (screen 5 = main menu)

/* =========================================================================
   Part 2 -- Load the slot-A save data the title screen would have.

   The load itself is the game's memory-card check (queued by 0x8003F23C, a
   35-state async task that mounts slot A, finds/CREATES the Mario Baseball
   file, reads it, and unmounts, staging through ARAM and scattering the data
   into the live save structures). It is the only practical loader -- the save
   is unpacked field-by-field into dozens of globals, so it cannot reasonably
   be reimplemented with raw SDK calls. The catch is that this task is a
   user-facing wizard that posts a dialog at nearly every state.
   ========================================================================= */
#define cardLoadModeByte    VAR_ADDRESS(u8, 0x80366177)   // set before the check
static inline void loadMemoryCardSlotA(void) { ((void(*)(void))0x8003F23C)(); }

// SDK CARDProbeEx(chan, *memSize, *sectorSize): 0 = card ready, -1 = busy
// (still detecting -- retry next frame), -3 = no card. Standalone presence
// check (the game uses it the same way at 0x800ABBA0); needs no mount.
#define CARD_RESULT_READY   0
#define CARD_RESULT_BUSY    (-1)
static inline int cardProbeExSlotA(void)
{
    int memSize, sectorSize;
    return ((int(*)(int, int*, int*))0x80086144)(0, &memSize, &sectorSize);
}

// The wizard's dialog lives in the card-check struct at 0x803C50E8:
//   +0x3e = "dialog active" flag the on-screen box render reads every frame
//   +0x44/+0x45 = the response fields the wizard polls for the user's answer
//                 (0 = no answer yet; the wizard branches on +0x45: 3-4 =
//                 proceed/confirm, so DLG_CONFIRM feeds that).
// Holding +0x3e at 0 hides the box; feeding a response auto-answers the
// no-data "create?" prompt so it does not block.
#define dlgActive           VAR_ADDRESS(u8, 0x803C5126)   // 0x803C50E8 + 0x3e
#define dlgResponseA        VAR_ADDRESS(u8, 0x803C512C)   // 0x803C50E8 + 0x44
#define dlgResponseB        VAR_ADDRESS(u8, 0x803C512D)   // 0x803C50E8 + 0x45
#define DLG_CONFIRM         3        // "proceed" branch of the wizard's poll

// How long to hide the box / auto-confirm after the wizard is fired. The load
// finishes within a few seconds; after the window, dialogs behave normally.
#define SUPPRESS_FRAMES     600      // ~10 s at 60 fps

// PERSISTENT "already loaded" latch: set once a load actually succeeds and
// NEVER cleared by us, so the load runs a single time per session instead of
// every time the menu rel reloads (menu->game->menu). It lives in DOL .bss,
// which is zeroed at cold boot / hard reset (a genuinely new session, where we
// do want to reload) but is NOT re-zeroed when only the menu rel reloads. NOTE:
// must NOT be 0x803C50E8 -- the card task reads that u8 as an abort flag.
#define g_loaded            VAR_ADDRESS(u8, 0x802EC01B)

// Per-menu-load probe phase, re-armed to 0 every rel==0 frame (so a boot with
// no card still re-checks on the next menu load if a card is later inserted,
// without re-probing every frame): 0 = probe, 1 = suppression window, 2 = idle.
#define g_phase             VAR_ADDRESS(u8, 0x802EC01E)
#define g_frames            VAR_ADDRESS(halfword, 0x802EC01C)

// No .address -> once per frame, and deliberately no .state: this code has to
// see rel 0 (to force the boot walk) AND rel 4 (to drive the card load), so it
// gates itself on `rel` internally instead.
CGECKO(BootToMainMenu);
void BootToMainMenu()
{
    // Boot straight to the main menu (skips intro + title screen). While the
    // boot window is open, re-arm the per-menu-load probe phase.
    if (inningSetting.rel == 0)
    {
        PatchInstruction(BOOT_TO_MENU_SITE, LI_R3_5);
        g_phase = 0;
    }

    if (inningSetting.rel != 4)
        return;

    // Load exactly once per session: once a load has succeeded, skip entirely
    // on every subsequent menu (re)load.
    if (g_loaded)
        return;

#if CARD_LOAD_METHOD == CARD_LOAD_GAME_WIZARD
    // Reference method: just fire the game's card check. Shows the box/prompts.
    if (g_phase == 0)
    {
        g_phase  = 2;
        g_loaded = 1;
        cardLoadModeByte = 1;
        loadMemoryCardSlotA();
    }
#else
    // Silent method.
    if (g_phase == 0)
    {
        int probe = cardProbeExSlotA();
        if (probe == CARD_RESULT_BUSY)
            return;                          // still detecting -- try next frame
        if (probe == CARD_RESULT_READY)
        {
            // Card present: load it, then hide the wizard's dialog for a while.
            cardLoadModeByte = 1;
            loadMemoryCardSlotA();
            g_frames = 0;
            g_phase  = 1;
        }
        else
        {
            // No card this menu load: go idle. g_loaded stays 0, so the next
            // menu (re)load re-probes (in case a card is inserted later).
            g_phase = 2;
        }
    }
    else if (g_phase == 1)
    {
        // Hide the wizard's box and auto-confirm its prompts while it loads or
        // creates. Harmless once the load is done (no dialog is pending), and
        // the window is short so normal dialogs are unaffected afterward.
        dlgActive    = 0;
        dlgResponseA = 1;
        dlgResponseB = DLG_CONFIRM;
        if (++g_frames >= SUPPRESS_FRAMES)
        {
            g_phase  = 2;
            g_loaded = 1;   // load done -- never load again this session
        }
    }
#endif
}
