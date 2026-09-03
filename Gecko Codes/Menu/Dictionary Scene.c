/*###########################################################
# Dictionary Scene
###########################################################*/
// Author: LittleCoaks
//
// The CUSTOM version of "Dictionary Replaces Records": same reroute + music
// fix, PLUS custom elements drawn on top of the stock Dictionary scene. Enable
// this OR "Dictionary Replaces Records", not both.
//
// Both codes are .state = MSSB_MENU: every address here is menus.rel code or
// menu-side state, only valid while rel == 4.
//
// Two parts:
//  * per-frame (below) -- reroute + music fix, shared with Dictionary Replaces
//                   Records via Include/Rio/DictionaryReroute.h.
//  * C2 @0x80693C44 -- AUGMENT the stock Dictionary handler: run our overlay at
//                   the top, then re-execute the original `stwu` and fall
//                   through, so the stock scene (background, its registered draw
//                   callbacks, state machine, input, exit) all keep running and
//                   we just ADD elements. Stock element draw callbacks live at
//                   0x80693A48, 0x80692930 (menus.rel) and 0x8005295C,
//                   0x80053FE8 (main.dol); the scene dispatch is screenFuncTable
//                   @0x806D71CC, scene 7's menuProcess sub-table @0x806F7934.

#include "Include/Rio/DictionaryReroute.h"

// NO payload statics -- cgecko reaches statics PIC-relative via r31; a static
// touched inside a helper (e.g. ScreenTextTick) faulted in testing. All state
// lives at absolute claimed-RAM addresses instead.
#define SCREENTEXT_NO_FLOAT
#define TEXT_SLOTS 4
#define TEXT_BUFFER_ADDR 0x802EC0E8   // glyph scratch, see ClaimedFreeMemory.h
#include "Include/Rio/ScreenText.h"

#include "Include/static/UnknownHomes_Static.h"
#include "Include/text/text_channel.h"
#define g_magic  VAR_ADDRESS(u32, 0x802EC270)   // one-shot init sentinel
#define g_toggle VAR_ADDRESS(u32, 0x802EC274)   // our toggle "button" state

// ---- reroute + music fix (every frame in menu state) ----------------------
CGECKO(DictionarySceneCommon, .state = MSSB_MENU);
void DictionarySceneCommon()
{
    DictionaryReroute_Tick();
}

// ---- C2: custom elements drawn on top of the stock scene ------------------
// .instruction re-runs the stock handler's own first instruction, so the scene
// body carries on normally after our overlay.
CGECKO(DictionaryScene, .address = 0x80693C44, .state = MSSB_MENU,
                        .instruction = "stwu r1, -48(r1)");
void DictionaryScene()
{
    if (g_magic != 0x0D1C7)          // init the toggle once (claimed RAM holds
    {                                // whatever was there at power-on)
        g_magic  = 0x0D1C7;
        g_toggle = 0;
    }

    // Spare button: the stock scene uses A/B/dpad, so Y is free for our toggle.
    // newInput is edge-detected by the menu (set only on the press frame),
    // exactly like Y-to-superstar on the batting-order screen.
    controllerInputStruct* in = Static_Stats_Tables.controllerInputs;
    if (in[0].newInput & INPUT_BUTTON_Y)
        g_toggle = !g_toggle;

    // Our added elements. Positions are low on screen to avoid the stock
    // layout; adjust once you can see it against the real scene.
    ScreenTextTick();
    WriteTextEx(320, 300, TEXT_WHITE, TEXT_LARGE, TEXT_CENTER, "Project Rio");
    WriteTextEx(320, 352, g_toggle ? TEXT_YELLOW : TEXT_GRAY, TEXT_SMALL, TEXT_CENTER,
                g_toggle ? "[*] Toggle: ON   (Y)"
                         : "[ ] Toggle: OFF  (Y)");
}
