/*###########################################################
# Options Menu
###########################################################*/
// Author: LittleCoaks

// *A custom mod-options menu: a scrollable list of toggles, navigated with
// *the D-pad and flipped with A. Each row owns one BYTE of memory
// *(Include/Rio/ModOptions.h) -- A writes it, the row's ON/OFF text reads it
// *back, and any other gecko code can read the same byte to gate itself.
// *Opened with the main menu's own Options button. No memory-card save yet.
//
// REPLACE hook, the same construction as Gecko Codes/Global/Custom Menu
// Scene.c: MSSB menus are a per-frame jump table (currentScreenFunctionChooser
// 0x8064026C reads menuCtrl->screenCode and calls screenFuncTable[sc]()), so a
// "scene" is just that one function. Overriding the re-executed instruction
// with `blr` returns to the dispatcher's `bctrl` the moment our body finishes,
// so the stock Options body (which begins `stwu r1,-16(r1)`) never runs at all
// and there is no stack leak. We ARE the Options scene now.
//
// Target: screenFuncTable[6] (Options), handler 0x80658D98, menus.rel. Nothing
// reroutes anything -- the main menu's Options button already does
// changeScreenVariables(6) (the `li r3,6` at 0x80641798), so the button opens
// this instead.
//
// .state = MSSB_MENU is REQUIRED: 0x80658D98 is menus.rel code, only valid
// while rel == 4. At rel == 5 that address is game.rel and must not be hooked.
//
// WHY REPLACE AND NOT AUGMENT. This code used to be a C2 with
// `.instruction = "stwu r1, -16(r1)"`, letting the stock Options screen keep
// running underneath and drawing our list on top of it. Live testing killed
// that approach on both counts:
//   * the cursor did not work -- our list and the stock screen's own option
//     widgets were both driven off controllerInputs_, and blanking the buttons
//     after we read them (which is what the old code did) was not enough to
//     separate them;
//   * the stock screen's art behind the list made the text unreadable.
// Replacing the handler fixes both at the source: no stock widget runs, so
// there is nothing to fight over the D-pad with and nothing drawn behind us.
// The cost is the stock Options screen itself -- audio/rumble settings and the
// memory-card save that its back-out confirm dialog triggers are gone while
// this code is enabled.
#include "Include/game/UnknownHomes_Game.h"

// TEXT SLOT BUDGET -- 11 blocks, the worst case for one frame:
//   1 title + 1 footer hint + 1 cursor glyph
//   + OPT_VISIBLE_ROWS * 2 (each row is a label block AND a value block)
// The label and the value are separate blocks, at fixed x, because the font is
// proportional -- padding one string out with spaces would leave the ON/OFF
// column ragged. Two blocks at two fixed x's line up exactly. TEXT_MAXLEN is
// trimmed to 19 (the longest string below is 18 glyphs) so the whole claim
// still fits under the Debug Mode block at 0x802EC504.
#define SCREENTEXT_NO_FLOAT
#define TEXT_SLOTS 11
#define TEXT_MAXLEN 19
#define TEXT_BUFFER_ADDR 0x802EC32C   // 448 bytes, see ClaimedFreeMemory.h

// OUR OWN FRAME COUNTER -- REQUIRED IN MENU CONTEXT, and the single reason the
// first two versions of this menu looked broken.
//
// ScreenText.h frees last frame's text blocks by noticing that its frame stamp
// changed, and its default stamp is g_d_GameSettings.FrameCountWhileNotAtMainMenu
// (u16 @0x800E8700). That counter reads 0 AND NEVER MOVES anywhere in the menu
// REL -- live-verified 2026-08-28 while this scene was on screen: the counter
// sat at 0 and ScreenText's own nextSlot sat at 11 (= TEXT_SLOTS, full). So
// ScreenTextTick() early-returned every frame, all 11 slots stayed claimed from
// the very first frame, and every WriteTextEx after that frame was silently
// dropped. The screen was a FROZEN SNAPSHOT of frame 1 -- which is why the
// cursor looked dead even though the D-pad and the A toggle were working the
// whole time (the option bytes were flipping in RAM behind the stuck picture).
//
// This scene runs exactly once per frame (the menu dispatcher calls
// screenFuncTable[6]() once), so counting our own calls is an exact frame
// clock. It has to be an ABSOLUTE claimed-RAM address, not a payload static:
// ScreenTextTick() is a helper, and cgecko reaches payload statics through r31,
// which read NULL from inside a helper while prototyping Custom Menu Scene.c.
#define g_frame VAR_ADDRESS(u32, 0x802EC4EC)
#define ScreenText_FrameNow g_frame

#include "Include/Rio/ScreenList.h"

#include "Include/Rio/ModOptions.h"
#include "Include/Local/Legacy.h"
// changeScreenVariables by raw address instead of Include/Local/LegacyMenus.h.
// That header binds the file to MENU context (Include/Symbols/menus.h sets
// MSSB_CONTEXT_MENUS, and game.h then #errors with "the game and menus RELs
// share one arena slot -- a mod cannot bind both"). This is the ONLY menus.rel
// symbol the scene needs, so taking it as an address keeps the file
// context-free and lets RioModPack include it alongside game-REL mods. The
// runtime guarantee is unchanged: the scene hook is .state = MSSB_MENU, so it
// only ever runs while menus.rel is the resident REL.
#define changeScreenVariables ((int (*)(int))0x80640234)

// NO PAYLOAD STATICS for MUTABLE state -- cgecko reaches payload statics
// PIC-relative through r31, and one read from inside a HELPER (ScreenTextTick)
// crashed with r31 = NULL while prototyping Custom Menu Scene.c. All mutable
// state lives at absolute claimed-RAM addresses. The read-only s_options table
// below is fine: it is .picdata like any string literal, its pointers are
// formed in the entry and passed to helpers as arguments.
#define g_magic VAR_ADDRESS(u32, 0x802EC308)
#define s_list  ((ScreenList*)0x802EC30C)          // 16 bytes

// ---- BACKGROUND -----------------------------------------------------------
// The main menu's Options button is literally
//     li r3, 6 ; bl changeScreenVariables      @0x80641798
// and then it falls straight to its epilogue -- NO teardown, and once
// screenCode is 6 the main-menu handler never runs again, so its screen is
// still fully drawn underneath us. (Because the button does nothing else,
// writing screenCode=6/menuProcess=0 by hand reproduces the real transition
// exactly -- that is how all of this was tested without a controller.)
//
// TWO DEAD ENDS, so nobody spends the afternoon on them again:
//   * changeScene_(1,0) (0x800204CC) only REQUESTS a scene by writing an id
//     to 0x803716AD. It never touches what is drawn. No effect.
//   * resetAllDrawingStructs() (0x800B0BE8) + fn_80009144() (0x80009144),
//     which is debug.rel's own recipe, HANGS the menu: the scene dispatcher
//     0x8064026C is itself a node in those banks, so wiping them stops
//     screenFuncTable[6]() from ever being called again (g_frame froze at 1),
//     and fn_80009144 only re-adds the two DOL base draw functions.
//     Silencing the menus.rel draw nodes individually changes NOTHING on
//     screen either -- they only build state; node 0x80009094 draws it all.
//
// NOT OUR BUG, measured: entering screen 6 and returning leaks one draw node
// per round trip (12 -> 16 over four). The STOCK Options screen does exactly
// the same with this code disabled, so it is the game's own bookkeeping. At 64
// node slots it would take ~50 round trips to matter. It also shows as a faint
// double-draw of the main menu's first item after a few trips.
//
// WHAT THE SCREEN IS ACTUALLY MADE OF (all traced live 2026-08-28):
//   * The frame loop walks a list of draw nodes (RunDrawScripts 0x800B0CB8);
//     silencing the menus.rel nodes in it changes NOTHING on screen -- they
//     only build/update state. Node 0x80009094 (priority 0x7000) draws the
//     entire 2D screen; pointing it at a `blr` blanks the menu AND our text.
//   * 0x80009094 is a two-line wrapper around maybeProcessUIUpdates
//     (0x80035168), which is
//         for (i = *(0x803CBC98); i < *(0x803CB814); i++)   // UI elements,
//             draw element i;                               // 0xC0 B records
//                                                           // from 0x8039C3E0
//         DrawTextOnCondition(1);          // 0x80010F2C  <- OUR TEXT
//     The element loop and the text call are SEPARATE and the text comes
//     last. So collapsing the loop's range draws no elements and still draws
//     our text: black background, menu intact.
// Setting the end bound to 0 is exactly what the pair of variables is for --
// the code above the loop nudges them up and down under a debug flag and
// clamps the end to 0x360 -- so this is using them, not corrupting them.
#define UI_DRAW_END      VAR_ADDRESS(u32, 0x803CB814)  // element loop end (864)
#define NOTHING_SAVED    0xFFFFFFFF

// Claimed RAM at 0x802EB000 -- inside the 2224-byte all-zero run at
// 0x802EAF90-0x802EB83F, verified live (stable and zero) before claiming,
// since this is the previously-unclaimed head of the lbl_802EAF80 block.
#define g_savedDrawEnd  VAR_ADDRESS(u32, 0x802EB000)
#define g_bgMagic       VAR_ADDRESS(u32, 0x802EB004)
#define BG_MAGIC        0x0B6D0FF

/* Blank everything the menu framework draws, keeping the text pass. */
static void BlankBackground(void)
{
    if (g_savedDrawEnd == NOTHING_SAVED && UI_DRAW_END != 0)
    {
        g_savedDrawEnd = UI_DRAW_END;    /* only ever saves the real bound */
        UI_DRAW_END    = 0;
    }
}

/* Give the framework its screen back. Safe to call when nothing is saved.
 * The 0x360 bound is the same clamp the game's own code applies, so a
 * corrupted save can never be written back as a bogus element count. */
static void RestoreBackground(void)
{
    u32 saved = g_savedDrawEnd;

    if (saved != NOTHING_SAVED && saved != 0 && saved <= 0x360)
        UI_DRAW_END = saved;
    g_savedDrawEnd = NOTHING_SAVED;
}

// menuCtrl->menuProcess. changeScreenVariables() zeroes it on every screen
// change, so "menuProcess == 0" is exactly "first frame of this entry" -- the
// same per-scene state var the stock handlers sub-dispatch on. Using it means
// the cursor resets to the top each time the screen is opened, without a
// separate latch that would have to detect leaving as well.
#define menuProcess VAR_ADDRESS(u16, VAR_ADDRESS(u32, menuControlVariables_ADDR) + 4)

// ---- layout (640x480) ----------------------------------------------------
#define OPT_VISIBLE_ROWS 4      // 5 options in 4 rows, so the list scrolls
#define OPT_CURSOR_X   170
#define OPT_LABEL_X    190
#define OPT_VALUE_X    440
#define OPT_TOP_Y      178
#define OPT_ROW_H       30

// ---- the rows ------------------------------------------------------------
// One row = one label and one BYTE. addr is a plain address, so a row can
// point anywhere: the rows below all target the ModOptions flag block (mod
// on/off switches other codes read), but pointing one straight at a game byte
// works the same way and needs no new code here.
//
// A writes onValue when the byte is 0, and 0 when it is anything else, so a
// byte whose "on" state is not 1 still toggles correctly.
typedef struct
{
    const char* label;      /* <= 18 glyphs, see TEXT_MAXLEN above */
    u32         addr;       /* the word this row toggles */
    u32         onValue;    /* what ON writes; OFF always writes 0 */
} ModOptionRow;

static const ModOptionRow s_options[] =
{
    { "Widescreen",         MODOPT_ADDR(MODOPT_WIDESCREEN),  1 },
    { "CPU Always Sprints", MODOPT_ADDR(MODOPT_CPU_SPRINT),  1 },
    { "Instant Randoms",    MODOPT_ADDR(MODOPT_INSTANT_RNG), 1 },
    { "Duplicate Chars",    MODOPT_ADDR(MODOPT_DUPLICATES),  1 },
    { "Superstar Mode",     MODOPT_ADDR(MODOPT_SUPERSTARS),  1 },
};
#define OPT_COUNT ((int)(sizeof(s_options) / sizeof(s_options[0])))

// ---------------------------------------------------------------------------
// Our scene control function -- called once per frame while screenCode == 6,
// in menu context (rel == 4). void(void); it owns the whole frame.
// ---------------------------------------------------------------------------
CGECKO(OptionsMenu, .address = 0x80658D98, .state = MSSB_MENU,
                    .instruction = "blr");
void OptionsMenu()
{
    controllerInputStruct* in;
    u16 pressed;
    int i, first, last;

    if (g_magic != 0x0D71104)           // claimed RAM holds whatever was there
    {                                    // at power-on -- zero the flags ONCE
        g_magic = 0x0D71104;             // per session, not on every entry, so
        g_savedDrawEnd = NOTHING_SAVED;  // the toggles survive leaving the menu
        ModOptions_Reset();
    }

    if (menuProcess == 0)               // first frame of this entry
    {
        menuProcess = 1;
        BlankBackground();
        ScreenList_Init(s_list, OPT_COUNT, OPT_VISIBLE_ROWS);
    }

    // Start the frame: advance our clock and let ScreenText free last frame's
    // blocks. Doing it BEFORE the B exit below is what makes our text vanish on
    // the way out instead of lingering for a frame.
    g_frame++;
    ScreenTextTick();

    // P1 input. newInput is already edge-detected by the menu's input gather
    // (a bit is set only on the frame the button is first pressed) -- reading
    // it back from an external tool shows 0, because the gather clears it again
    // the same frame; it is live exactly where we read it. Nothing is consumed
    // or blanked here -- no stock widget is running to fight with.
    in = (controllerInputStruct*)controllerInputs__ADDR;
    pressed = in[0].newInput;

    if (pressed & INPUT_BUTTON_B)
    {
        RestoreBackground();            // give the framework its screen back
        changeScreenVariables(5);       // ...then hand over to the main menu
        return;                          // draw nothing on the exit frame
    }
    if (pressed & INPUT_BUTTON_DOWN)
        ScreenList_MoveDown(s_list);
    if (pressed & INPUT_BUTTON_UP)
        ScreenList_MoveUp(s_list);
    if (pressed & INPUT_BUTTON_A)
    {
        const ModOptionRow* row = &s_options[s_list->selected];
        u32* flag = (u32*)row->addr;
        *flag = *flag ? 0 : row->onValue;
    }

    // ---- draw ------------------------------------------------------------
    // KNOWN LIMITATION: the previous screen (normally the main menu) is still
    // drawn behind this list -- see the changeScene_/resetAllDrawingStructs
    // notes above for the two levers that were tried and why neither worked.
    // What is left, roughly cheapest first:
    //   1. draw each string twice, black and offset by a pixel, so it reads
    //      against anything (costs 2 text slots per string, so fewer rows);
    //   2. hook insertGraphicDrawingFunction (0x800B0A5C) while the stock
    //      Options scene sets itself up and reject only its WIDGET callbacks,
    //      keeping its background element -- this is the one that gives a real
    //      menu background, and it needs the Options element callbacks
    //      identified the way the Dictionary scene's were;
    //   3. reset the drawing banks properly, re-registering the menu
    //      dispatcher node along with the DOL base functions.
    //
    // The rows are drawn here rather than through ScreenList_Draw because each
    // one needs a second block for its ON/OFF value; the scroll and cursor
    // bookkeeping above is still the widget's (ScreenList_Move*), this is only
    // its Draw half specialised. Selected row = yellow with a ">" beside it,
    // the same read as the debug menu's own lists. (ScreenTextTick already ran
    // at the top of the frame, above.)
    WriteTextEx(320, 110, TEXT_WHITE, TEXT_LARGE, TEXT_CENTER, "MOD OPTIONS");

    first = s_list->scrollTop;
    last  = first + s_list->visibleRows;
    if (last > OPT_COUNT)
        last = OPT_COUNT;

    for (i = first; i < last; i++)
    {
        int  y   = OPT_TOP_Y + (i - first) * OPT_ROW_H;
        bool sel = (i == s_list->selected);
        bool on  = (*(u32*)s_options[i].addr != 0);

        if (sel)
            WriteTextEx(OPT_CURSOR_X, y, TEXT_YELLOW, TEXT_SMALL, TEXT_LEFT, ">");
        WriteTextEx(OPT_LABEL_X, y, sel ? TEXT_YELLOW : TEXT_WHITE,
                    TEXT_SMALL, TEXT_LEFT, "%s", s_options[i].label);
        WriteTextEx(OPT_VALUE_X, y, on ? TEXT_GREEN : TEXT_GRAY,
                    TEXT_SMALL, TEXT_LEFT, on ? "ON" : "OFF");
    }

    WriteTextEx(320, 330, TEXT_GRAY, TEXT_SMALL, TEXT_CENTER, "A: TOGGLE  B: BACK");
}

// ---------------------------------------------------------------------------
// Safety net for the way OUT.
//
// The scene blanks the whole menu framework while it is open and un-blanks it
// in the B handler. Leaving by ANY other route -- a crash, a savestate loaded
// with the screen open, some future code changing screens itself -- would
// otherwise strand UI_DRAW_END at 0 and leave EVERY menu in the game blank
// with no way to get it back short of a reboot. This runs every frame and
// undoes it the moment the Options screen is not the current screen.
//
// MSSB_ALWAYS, not MSSB_MENU: the blanked variable is main.dol state shared
// with the match, so the restore has to be able to fire in any state. It only
// reads a DOL pointer, and it is a no-op whenever nothing is saved, which is
// almost always.
//
// g_bgMagic is initialised HERE rather than in the scene, because this code
// runs from boot: claimed RAM holds whatever was there at power-on, and
// without a one-shot init the very first frame could "restore" garbage into
// UI_DRAW_END and blank the game before the scene had ever run.
CGECKO(OptionsMenuRestore, .state = MSSB_ALWAYS);
void OptionsMenuRestore()
{
    u32 ctrl;

    if (g_bgMagic != BG_MAGIC)          // one-shot, before any restore can run
    {
        g_bgMagic      = BG_MAGIC;
        g_savedDrawEnd = NOTHING_SAVED;
        return;
    }
    if (g_savedDrawEnd == NOTHING_SAVED) // nothing blanked -- the common case
        return;

    ctrl = VAR_ADDRESS(u32, menuControlVariables_ADDR);
    if (ctrl < 0x80000000 || ctrl >= 0x81800000)
        return;                          // no menu control struct yet (boot)
    if (VAR_ADDRESS(u16, ctrl + 2) != 6) // screenCode -- we are not the screen
    {
        RestoreBackground();
        // ...and take our text down with it. The B handler does this for free
        // (it ticks, then returns without drawing, so the blocks are released
        // that frame), but an abnormal exit never runs it -- verified live:
        // forcing screenCode away left "MOD OPTIONS" painted over the restored
        // main menu. Ticking a fresh frame here releases the blocks.
        g_frame++;
        ScreenTextTick();
    }
}
