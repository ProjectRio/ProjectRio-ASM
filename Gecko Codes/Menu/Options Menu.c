/*###########################################################
# Options Menu
###########################################################*/
// Author: LittleCoaks

// *Stage 1 of the custom mod-options menu: draws a scrollable list of
// *placeholder toggles ON TOP of the stock Options screen, navigated with
// *the D-pad. No real toggle state or memory-card save yet -- see the
// *approved plan (glowing-rolling-book.md) for Stages 2-4.
//
// AUGMENT hook, same shape as Gecko Codes/Menu/Dictionary Scene.c: the C2
// re-executes the stock handler's own first instruction, so its 7-state
// body (background/scene setup, the confirm-dialog backout, the memory-card
// save that dialog triggers) all keeps running untouched underneath. We
// only ADD a list drawn on top and read the D-pad for it.
//
// Target: screenFuncTable[6] (Options), handler 0x80658D98, menus.rel --
// live-verified 2026-07-30 while idling on the real screen (state machine:
// local state var 0x80751B1C, jump table 0x806D9B9C; states 0-3 = scene
// setup; state5/6 = confirm dialog; state4 @0x80658F78 = the actual
// backout, calling changeScreenVariables(5) at 0x80658FAC).
//
// VERIFY LIVE: the stock screen's own video/audio option widgets may read
// D-pad input through their own registered draw callbacks (not visible in
// this handler's own disassembly), separately from our read below -- if
// Up/Down here ALSO moves something in the kept-original background, that
// needs a follow-up (no-op just that widget's input read), not abandoning
// this approach.
//
// The first build of this code crashed on entry -- that turned out to be a
// cgecko bug (payload data-section pointers were never relocated), now
// fixed in cgecko itself. Nothing in this file works around it.
#include "Include/game/UnknownHomes_Game.h"

// 5 slots: 1 title + up to 2 for the selected row (cursor + label) + 1 each
// for the other 2 rows, worst case in one frame = 1 + 2 + 1 + 1 = 5.
#define SCREENTEXT_NO_FLOAT
#define TEXT_SLOTS 5
#define TEXT_BUFFER_ADDR 0x802EC31C   // 488 bytes, see ClaimedFreeMemory.h
#include "Include/Rio/ScreenList.h"

#include "Include/Local/Legacy.h"
// Claimed RAM (no payload statics inside a menu C2 hook -- see ScreenList.h
// and project memory `menu-scene-dispatch` for why).
#define g_magic VAR_ADDRESS(u32, 0x802EC308)
#define s_list  ((ScreenList*)0x802EC30C)          // 16 bytes

CGECKO(OptionsMenu, .address = 0x80658D98, .state = MSSB_MENU,
                    .instruction = "stwu r1, -16(r1)");
void OptionsMenu()
{
    if (g_magic != 0x0D71104)          // one-shot init (claimed RAM holds
    {                                   // whatever was there at power-on)
        g_magic = 0x0D71104;
        ScreenList_Init(s_list, 3, 3); // 3 placeholder items, all visible
    }

    // newInput is edge-detected by the menu's own input gather (set only on
    // the press frame), same as every other menu-scene hijack in this repo.
    controllerInputStruct* in = (controllerInputStruct*)controllerInputs__ADDR;
    u16 pressed = in[0].newInput;

    if (pressed & INPUT_BUTTON_DOWN)
        ScreenList_MoveDown(s_list);
    if (pressed & INPUT_BUTTON_UP)
        ScreenList_MoveUp(s_list);

    // CONSUME the navigation buttons. We run at the TOP of the stock handler,
    // and its own widget code (0x80658840, reached from state 3) reads this
    // SAME struct later in the frame -- verified live: without this, one
    // D-pad press moves our list AND the stock Music/Sound/Rumble selection
    // at the same time. Clearing the bits after we've read them leaves the
    // stock screen seeing no input, so it stops reacting.
    //
    // B is deliberately NOT consumed: the stock back-out path (state 4 ->
    // changeScreenVariables(5) at 0x80658FAC) is exactly what we want to keep,
    // since the memory-card save rides on it. A is consumed because Stage 2
    // will use it to toggle an option -- letting it through would also fire
    // the stock confirm dialog.
    const u16 consumed = INPUT_BUTTON_LEFT | INPUT_BUTTON_RIGHT
                       | INPUT_BUTTON_DOWN | INPUT_BUTTON_UP
                       | INPUT_BUTTON_A;
    for (int p = 0; p < 2; p++)      // controllerInputs_ is a 2-entry array
    {
        in[p].currentHeldInput &= ~consumed;
        in[p].newInput         &= ~consumed;
        in[p].processedInput   &= ~consumed;
    }

    const char* items[] = { "Widescreen", "CPU Sprint Boost", "Instant Randoms" };

    ScreenTextTick();
    WriteTextEx(320, 80, TEXT_WHITE, TEXT_LARGE, TEXT_CENTER, "MOD OPTIONS (WIP)");
    ScreenList_Draw(s_list, 220, 140, 24, TEXT_SMALL, items);
}
