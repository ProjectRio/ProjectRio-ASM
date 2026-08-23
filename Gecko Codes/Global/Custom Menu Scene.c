/*###########################################################
# Custom Menu Scene (hello-world)
###########################################################*/
// Author: LittleCoaks
//
// Proves the menu-scene hijack end to end. MSSB menus are a per-frame
// jump table: currentScreenFunctionChooser (0x8064026C) does
//     sc  = menuControlStruct->screenCode;      // *(0x803CBBCC) + 2
//     fn  = screenFuncTable[sc];                // table @ 0x806D71CC
//     fn();                                     // called every frame
// So a "scene" is just screenFuncTable[screenCode](). We replace ONE
// scene's control function with our own.
//
// TARGET: the Records scene, screenCode 8, handler 0x8069D198. The main
// menu's Records button already calls changeScreenVariables(8), so no
// reroute is needed -- open Records and you land in this scene.
//
// HOW THE REPLACE IS CLEAN: a cgecko C2 runs
//     backup -> our payload -> restore -> [.instruction] -> branch back.
// Overriding the re-executed instruction to `blr` returns to the menu
// dispatcher's `bctrl` the instant our scene finishes, so the stock
// Records body (which begins `stwu r1,-80(r1)`) never runs and there is
// no stack leak. The dispatcher's `screenFuncTable[8]()` call now runs
// only our code and returns normally.
//
// .state = MSSB_MENU is REQUIRED -- 0x8069D198 is menus.rel code, only
// valid while rel==4. At rel==5 that address is game.rel and must not be
// hooked.
//
// ---- To retarget the UNUSED "Dictionary" scene (screenCode 7) instead ----
//   * change the hook's .address below to 0x80693C44 (screenFuncTable[7])
//   * add a companion, menu-gated code that reroutes the Records button --
//     a `# State: Menu` .asm with a single instruction "li r3, 7" at address
//     0x80641848 (cgecko emits it as the gated 04 write 04641848 38600007).
//     Dictionary(7) is dormant, so its handler is dead code -- replacing
//     it keeps the real Records screen intact. (Its menu-framework entry
//     transition is unexercised, though, which is why this first proof
//     uses the known-good Records slot.)

// TODO(decomp-migration): Menu data came from the Ghidra-exported MenuData.h.
// Replace with the decomp headers under Include/game/ and remap the
// symbol names -- see Include/_sync_report.txt and the decomp's
// docs/ghidra_name_conflicts.md.

// A private per-frame counter for ScreenText's slot bookkeeping. The
// default (FrameCountWhileNotAtMainMenu) works on submenus, but owning
// IMPORTANT -- NO PAYLOAD STATICS. cgecko accesses payload .data statics
// PIC-relative through r31; a static read from inside a HELPER (e.g.
// ScreenTextTick reading a custom frame counter) dereferenced a NULL base
// and crashed in testing (DAR 0x60 at 0x80002A48). So ALL scene state
// lives at absolute claimed-RAM addresses,
// and we use ScreenText's default frame counter (FrameCountWhileNotAtMainMenu,
// an absolute game var -- valid on the Records submenu). String literals are
// safe because their pointers are computed in the entry and passed as args.
//
// Keep the C2 payload small (well under the ~546-u32 cliff documented in
// Boot Directly To Game). SCREENTEXT_NO_FLOAT drops the %f formatter and its
// FPU save/restore (we only use %d); TEXT_BUFFER_ADDR moves the glyph
// scratch buffers into claimed RAM instead of embedding them in the payload.
#define SCREENTEXT_NO_FLOAT
#define TEXT_SLOTS 4
#define TEXT_BUFFER_ADDR 0x802EC0E8   // 392 bytes, see ClaimedFreeMemory.h
#include "Include/Rio/ScreenText.h"

#include "Include/Local/Legacy.h"
#include "Include/Local/LegacyMenus.h"
// Scene state in claimed RAM (absolute, no PIC). g_magic one-shot-inits the
// counter, since claimed RAM holds whatever was there at power-on.
#define g_magic  VAR_ADDRESS(u32, 0x802EC270)
#define g_aCount VAR_ADDRESS(s32, 0x802EC274)

// ---------------------------------------------------------------------------
// Our custom scene control function -- called once per frame while
// screenCode == 8, in menu context (rel==4). void(void); owns its own frame.
// ---------------------------------------------------------------------------
CGECKO(CustomMenuScene, .address = 0x8069D198, .state = MSSB_MENU,
                        .instruction = "blr");
void CustomMenuScene()
{
    if (g_magic != 0x5CE7E)         // one-shot init of the claimed-RAM counter
    {
        g_magic  = 0x5CE7E;
        g_aCount = 0;
    }

    // P1 input. newInput is already edge-detected by the menu's input
    // gather (a bit is set only on the frame the button is first pressed).
    controllerInputStruct* in = (controllerInputStruct*)controllerInputs__ADDR;
    u16 pressed = in[0].newInput;

    if (pressed & INPUT_BUTTON_A)
        g_aCount++;

    if (pressed & INPUT_BUTTON_B)
    {
        changeScreenVariables(5);   // back to the main menu
        return;                     // draw nothing on the exit frame
    }

    // Draw our scene (group 5, rendered by the menu's scene draw pass).
    ScreenTextTick();
    WriteTextEx(320, 120, TEXT_WHITE,  TEXT_LARGE, TEXT_CENTER, "CUSTOM MENU SCENE");
    WriteTextEx(320, 172, TEXT_GREEN,  TEXT_SMALL, TEXT_CENTER, "Menu-scene hijack works!");
    WriteTextEx(320, 214, TEXT_YELLOW, TEXT_SMALL, TEXT_CENTER, "A pressed %d times", g_aCount);
    WriteTextEx(320, 262, TEXT_WHITE,  TEXT_SMALL, TEXT_CENTER, "Press B to return");
}
