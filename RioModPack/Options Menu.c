/*###########################################################
# Options Menu
###########################################################*/
// Author: LittleCoaks

// *A custom mod-options menu: a scrollable list of toggles, navigated with
// *the D-pad and flipped with A. Each row owns one BYTE of memory
// *(RioModPack/ModOptions.h) -- A writes it, the row's ON/OFF text reads it
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

// TEXT SLOT BUDGET -- 22 blocks, the worst case for one frame. Both screens
// land near it by different routes:
//   options  1 title + 1 footer + 1 cursor + OPT_VISIBLE_ROWS*2 + NOTES_MAX_LINES
//            = 3 + 10 + 9 = 22
//   music    1 title + 1 footer + 1 cursor + MUSIC_VISIBLE_ROWS*2
//            = 3 + 18 = 21
// The label and the value are separate blocks, at fixed x, because the font is
// proportional -- padding one string out with spaces would leave the ON/OFF
// column ragged. Two blocks at two fixed x's line up exactly.
//
// TEXT_MAXLEN 27 is the longest string drawn: the notes panel is 24 glyphs
// wide, and every label, the title and the footer are shorter. The buffer is
// 8 + TEXT_SLOTS*(TEXT_MAXLEN+1)*2 = 1240 bytes.
//
// WHAT CAPS THIS. Two things, and the buffer is nearly at both:
//   * RAM. Everything the pack's UI and Custom Music claim shares one verified-
//     free run, 0x802EAF90-0x802EB83F (2032 bytes usable from 0x802EB050).
//     1240 buffer + 4 g_frame + 4 g_page + 16 s_music + 64 music config +
//     4 magic + 120 saved descriptors + 480 path buffers = 1932. To grow much
//     further the 480 bytes of path buffers would have to move out -- the old
//     0x802EC32C block is free again and nearly big enough.
//   * The game only has 30 text blocks. TEXT_FIRST_BLOCK is 30 - TEXT_SLOTS, so
//     22 already leaves the game only 8.
#define SCREENTEXT_NO_FLOAT
#define TEXT_SLOTS 22
#define TEXT_MAXLEN 27
#define TEXT_BUFFER_ADDR 0x802EB050   // 1240 bytes, see ClaimedFreeMemory.h

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
#define g_frame VAR_ADDRESS(u32, 0x802EB528)   // right after the text buffer
#define ScreenText_FrameNow g_frame

#include "Include/Rio/ScreenList.h"

#include "RioModPack/ModOptions.h"
#include "RioModPack/MusicConfig.h"
#include "Include/static/UnknownHomes_Static.h"
#include "Include/menus/yd_step.h"
#include "Include/text/text_channel.h"
#include "Include/Symbols/dol.h"          // menuControlVariables_ADDR
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

// ---- the music sub-screen -------------------------------------------------
// One option needs more than an on/off byte: the soundtrack has eight
// independent settings (menu + seven stadiums), so its row opens a screen of
// its own instead of toggling. g_page says which screen this frame draws; both
// live in the same scene function, because the menu dispatcher only calls us
// once and a second scene would need a second screenCode to be reachable.
#define PAGE_OPTIONS 0
#define PAGE_MUSIC   1
#define g_page   VAR_ADDRESS(u32, 0x802EB52C)
#define s_music  ((ScreenList*)0x802EB530)          // 16 bytes

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

// ---- layout (the full 4:3 frame) ------------------------------------------
// Deliberately NOT centred as a block: the screen is treated as a page that
// fills the frame, anchored top-left, because the content only grows -- more
// mods on the left, longer descriptions on the right. Centring a growing list
// means every addition shifts everything that was already there.
//
// The x's are set by measuring the widest string in each column against the
// font, which is proportional and runs about 12 px per glyph at TEXT_SMALL:
//   label  "Duplicate Chars" from 52 ends near 235, so the value column starts
//          at 250 -- at 230 the two collided on screen;
//   value  "OFF" from 250 ends near 287, clear of the notes at 310;
//   notes  NOTES_MAX_CHARS glyphs from 310 ends near 610, inside the frame.
// Widen a column and the next one has to move; the numbers are not arbitrary.
//
// The y's run from the title just under the top of the frame to the footer just
// above the bottom. 640x448 is the XFB, so nothing may reach either edge -- a
// TV overscans -- but there is far more room than the old block in the middle
// used, and it is all given to rows and description lines.
#define OPT_VISIBLE_ROWS 5
#define OPT_CURSOR_X    32
#define OPT_LABEL_X     52
#define OPT_VALUE_X    250
#define OPT_TOP_Y      108
#define OPT_ROW_H       34

#define OPT_TITLE_X     32
#define OPT_TITLE_Y     52
#define OPT_FOOTER_Y   396

// The notes panel.
//   NOTES_MAX_LINES caps what one mod can push onto the screen: past it the
//   text would run into the footer, and every line costs a text slot out of
//   the budget above.
//   NOTES_MAX_CHARS is the panel's own width, and DrawNotes word-WRAPS to it:
//   a mod can write its .notes as ordinary prose and it fills the panel. An
//   explicit newline in the .notes is still honoured as a hard break, so a mod
//   that wants its own layout keeps it. Wrapping at the panel width rather
//   than relying on TEXT_MAXLEN is what keeps the text inside the frame.
//
//   NOT ScreenText's own WriteTextWrapped(): that packs a whole paragraph into
//   ONE text block, so TEXT_MAXLEN would have to cover the entire note -- at
//   ~160 glyphs the buffer becomes 8 + 17*161*2 = 5482 bytes, far more claimed
//   RAM than the region has left. One block per line keeps TEXT_MAXLEN at 27.
//
//   A note longer than NOTES_MAX_LINES * NOTES_MAX_CHARS is trimmed and ends
//   in "...". Nine lines of 24 is about 200 glyphs, which is the practical
//   ceiling: another line costs another text slot, and the budget above is
//   already close to both of its limits.
//
//   AVOID '_' IN A NOTE. The font has no underscore: ScreenText_Encode() maps
//   0x21-0x5B, ']', '^', '{}~' and lowercase, and everything else -- 0x5F
//   included -- falls through to glyph 30, which is '?'. A filename written
//   with underscores comes out as "custom?01?h.adp" on screen. The ini
//   description has no such limit, so spell them out there instead.
#define NOTES_X        310
#define NOTES_TOP_Y    108
#define NOTES_LINE_H    26
#define NOTES_MAX_LINES  9
#define NOTES_MAX_CHARS 24

// ---- the rows ------------------------------------------------------------
// One row = one label and one BYTE. addr is a plain address, so a row can
// point anywhere: the rows below all target the ModOptions flag block (mod
// on/off switches other codes read), but pointing one straight at a game byte
// works the same way and needs no new code here.
//
// A writes onValue when the byte is 0, and 0 when it is anything else, so a
// byte whose "on" state is not 1 still toggles correctly.
//
// A row whose `page` is not PAGE_OPTIONS does not toggle at all: A opens that
// screen instead. Its `addr` is still the mod's option word, because that is
// the key the notes panel looks up -- the row needs it even when nothing about
// it is on or off.
typedef struct
{
    const char* label;      /* <= 18 glyphs, see TEXT_MAXLEN above */
    u32         addr;       /* the word this row toggles, or just its notes key */
    u32         onValue;    /* what ON writes; OFF always writes 0 */
    u32         page;       /* PAGE_OPTIONS = a toggle; else the screen A opens */
} ModOptionRow;

// Only the mods RioModPack actually ships get a row. A row for a mod that is
// not in the pack is a switch wired to nothing: it flips its byte and the
// screen says ON, but no code reads it. The ids in ModOptions.h are still
// reserved for the rest -- an id IS its offset, so they must not be renumbered
// -- they just have no row until their mod is included.
static const ModOptionRow s_options[] =
{
    { "Widescreen",      MODOPT_ADDR(MODOPT_WIDESCREEN), 1, PAGE_OPTIONS },
    { "Duplicate Chars", MODOPT_ADDR(MODOPT_DUPLICATES), 1, PAGE_OPTIONS },
    { "Custom Music",    MODOPT_ADDR(MODOPT_MUSIC),      1, PAGE_MUSIC   },
    { "Gecko Codes",     MODOPT_ADDR(MODOPT_GECKO),      1, PAGE_OPTIONS },
    { "Night Stadium",   MODOPT_ADDR(MODOPT_NIGHT_MARIO), 1, PAGE_OPTIONS },
};
#define OPT_COUNT ((int)(sizeof(s_options) / sizeof(s_options[0])))

// ---- the music screen -----------------------------------------------------
// Nine of the sixteen slots at a time; ScreenList scrolls the rest. Nine is
// what the text budget allows: title + footer + cursor + 9*2 = 21, just under
// TEXT_SLOTS. Showing all sixteen would need 35, more than the game has text
// blocks in total.
//
// Tighter rows than the options screen, because this list has no notes panel
// beside it and can use the height for rows instead.
#define MUSIC_VISIBLE_ROWS 9
#define MUSIC_TRACK_X    250
#define MUSIC_TOP_Y      108
#define MUSIC_ROW_H       30
#define MUSIC_FOOTER_Y   396

// ---- the notes panel ------------------------------------------------------
// The selected mod's description, drawn down the right half of the screen.
//
// The text is not stored here. It comes from the .notes the mod itself
// declares on its CGECKO() line, which cgecko collects into a table keyed by
// the option word -- the same word the row already toggles, so the row needs
// nothing new to find it. See CGecko_NotesForOption in CGecko/Common.h.
//
// A gecko build has no such table (each hook links separately, so one mod
// cannot see another's) and the stub there returns 0; this menu then simply
// draws no notes rather than failing to build. In the DOL-baked pack, where
// every mod is one translation unit, the real table is there.
//
// Long text is word-wrapped to the panel width; a '\n' the mod wrote is still
// a hard break, so prose and hand-laid-out notes both work. `buf` is a LOCAL --
// the "no payload statics" rule at the top of this file is about MUTABLE
// state reached through r31 from a helper; stack is fine, and this runs in
// the hook's own frame.
static void DrawNotes(u32 optionAddr)
{
    const char* note = CGecko_NotesForOption(optionAddr);
    int line;

    if (note == 0)
        return;                      // mod declared no .notes, or gecko build

    for (line = 0; line < NOTES_MAX_LINES; line++)
    {
        char buf[NOTES_MAX_CHARS + 1];
        int  take, lastSpace, i;

        while (*note == ' ')                 // the space we broke at
            note++;
        if (*note == '\n')                   // a break the author asked for
            note++;
        while (*note == ' ')
            note++;
        if (*note == 0)
            break;

        // How much of the rest fits on this line, remembering the last space
        // we passed so a word that straddles the edge can move down whole.
        take = 0;
        lastSpace = -1;
        while (note[take] != 0 && note[take] != '\n' && take < NOTES_MAX_CHARS)
        {
            if (note[take] == ' ')
                lastSpace = take;
            take++;
        }
        if (take == NOTES_MAX_CHARS && note[take] != 0 && note[take] != '\n' &&
            note[take] != ' ' && lastSpace > 0)
            take = lastSpace;                // we stopped mid-word: back up

        for (i = 0; i < take; i++)
            buf[i] = note[i];
        buf[take] = 0;
        note += take;

        // Out of lines with text still to come: end in "..." rather than
        // stopping mid-sentence, so a note that is too long reads as trimmed
        // instead of as a mod that garbled its own description.
        if (line == NOTES_MAX_LINES - 1)
        {
            const char* rest = note;

            while (*rest == ' ' || *rest == '\n')
                rest++;
            if (*rest != 0)
            {
                int at = (take <= NOTES_MAX_CHARS - 3) ? take : NOTES_MAX_CHARS - 3;

                buf[at]     = '.';
                buf[at + 1] = '.';
                buf[at + 2] = '.';
                buf[at + 3] = 0;
            }
        }

        WriteTextEx(NOTES_X, NOTES_TOP_Y + line * NOTES_LINE_H,
                    TEXT_WHITE, TEXT_SMALL, TEXT_LEFT, "%s", buf);
    }
}

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
        g_page = PAGE_OPTIONS;          // always open on the options list
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
    in = Static_Stats_Tables.controllerInputs;
    pressed = in[0].newInput;

    // ---- the music screen -------------------------------------------------
    // B goes back to the options list rather than out of the scene, so the two
    // screens nest the way the rest of the game's menus do.
    if (g_page == PAGE_MUSIC)
    {
        char scratch[MUSIC_PATHBUF_SIZE];   // MusicTrackStep builds paths here
        int  slot;

        if (pressed & INPUT_BUTTON_B)
        {
            g_page = PAGE_OPTIONS;
            return;                      // draw nothing on the frame we leave
        }
        if (pressed & INPUT_BUTTON_DOWN)
            ScreenList_MoveDown(s_music);
        if (pressed & INPUT_BUTTON_UP)
            ScreenList_MoveUp(s_music);

        // Left/Right walk the track list for the highlighted slot. MusicTrackStep
        // skips any track whose file is not on this disc, so a custom slot with
        // nothing behind it is never offered -- which is also why there is no
        // "missing" state to draw.
        slot = s_music->selected;
        if (pressed & INPUT_BUTTON_RIGHT)
            MusicSlot(slot) = MusicTrackStep(slot, MusicSlotTrack(slot), +1, scratch);
        if (pressed & INPUT_BUTTON_LEFT)
            MusicSlot(slot) = MusicTrackStep(slot, MusicSlotTrack(slot), -1, scratch);

        WriteTextEx(OPT_TITLE_X, OPT_TITLE_Y, TEXT_WHITE, TEXT_LARGE, TEXT_LEFT,
                    "MUSIC");

        first = s_music->scrollTop;
        last  = first + s_music->visibleRows;
        if (last > MUSIC_SLOT_COUNT)
            last = MUSIC_SLOT_COUNT;

        for (i = first; i < last; i++)
        {
            int  y   = MUSIC_TOP_Y + (i - first) * MUSIC_ROW_H;
            bool sel = (i == s_music->selected);
            u32  trk = MusicSlotTrack(i);

            // Red means "configured, but that file is not on this disc, so the
            // mod is ignoring it and the stock music plays". Left/Right can
            // never put a slot into that state -- MusicTrackStep skips absent
            // files -- but the config is plain RAM, so an ini code or a word
            // left over from another build can, and saying so beats drawing it
            // the same green as a track that really is playing.
            u32 live = (trk == MUSIC_DEFAULT) || MusicTrackAvailable(i, trk, scratch);

            if (sel)
                WriteTextEx(OPT_CURSOR_X, y, TEXT_YELLOW, TEXT_SMALL, TEXT_LEFT, ">");
            WriteTextEx(OPT_LABEL_X, y, sel ? TEXT_YELLOW : TEXT_WHITE,
                        TEXT_SMALL, TEXT_LEFT, "%s", s_musicSlotLabel[i]);
            WriteTextEx(MUSIC_TRACK_X, y,
                        !live ? TEXT_RED : (trk == MUSIC_DEFAULT) ? TEXT_GRAY : TEXT_GREEN,
                        TEXT_SMALL, TEXT_LEFT, "%s", s_musicTrackLabel[trk]);
        }

        WriteTextEx(OPT_TITLE_X, MUSIC_FOOTER_Y, TEXT_GRAY, TEXT_SMALL, TEXT_LEFT,
                    "L/R: TRACK  B: BACK");
        return;
    }

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

        if (row->page != PAGE_OPTIONS)
        {
            g_page = row->page;
            ScreenList_Init(s_music, MUSIC_SLOT_COUNT, MUSIC_VISIBLE_ROWS);
            return;                      // the new screen draws from next frame
        }
        {
            u32* flag = (u32*)row->addr;
            *flag = *flag ? 0 : row->onValue;
        }
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
    WriteTextEx(OPT_TITLE_X, OPT_TITLE_Y, TEXT_WHITE, TEXT_LARGE, TEXT_LEFT,
                "MOD OPTIONS");

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
        // A row that opens a screen has no on/off state to show; "SET" says
        // that A does something other than flip it.
        if (s_options[i].page != PAGE_OPTIONS)
            WriteTextEx(OPT_VALUE_X, y, TEXT_GRAY, TEXT_SMALL, TEXT_LEFT, "SET");
        else
            WriteTextEx(OPT_VALUE_X, y, on ? TEXT_GREEN : TEXT_GRAY,
                        TEXT_SMALL, TEXT_LEFT, on ? "ON" : "OFF");
    }

    DrawNotes(s_options[s_list->selected].addr);

    WriteTextEx(OPT_TITLE_X, OPT_FOOTER_Y, TEXT_GRAY, TEXT_SMALL, TEXT_LEFT,
                "A: SELECT  B: BACK");
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
