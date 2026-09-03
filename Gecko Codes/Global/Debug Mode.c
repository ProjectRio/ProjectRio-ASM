/*###########################################################
# Load Challenge REL   (loads debug.rel over the menu REL)
###########################################################*/
// Author: LittleCoaks
//
// Boots the game into the UNUSED debug.rel instead of the menu REL, and
// puts the missing TEXT on screen.
// (The repo used to call this module challenge.rel; the decomp has since
// renamed it -- it is the game's leftover developer DEBUG MENU. Same
// module either way: module id 1, the third LZ blob in aaaa.dat.)
//
// The codes, in the order the sections below define them:
//   1. load_debug_rel_over_menus -- three word writes; that is the whole
//      loader. Everything the old version did on top of it (post-prolog
//      glue, a VI-callback frame pump, forced node sequencing,
//      pre-launching entry 0's UI framework) is gone: see "WHY NO GLUE".
//   2. debug_menu_capture_state  -- 1-line hook that records the argument
//      of the DOL's compiled-out menu renderer.
//   3. debug_menu_text           -- draws the top-level entry list (which
//      the suite never draws at all) and the in-scene menus' item text
//      (whose renderer is that compiled-out stub).
//   4. nop_gx_thread_panic_a/b   -- silence the suite's off-thread GX assert.
//   5. skn_null_guard            -- entry 4 draws a skinned actor with no skin.
//   6. debug_rel_code_patches    -- three words in debug.rel itself, written
//      per frame while it is the loaded module, that fix entry 1's two crashes.
//   7. gfx_remove_empty_guard    -- stop the DOL corrupting its graphics table
//      when a scene removes an element that owns no slots.
//
// ===========================================================================
// PART 1 -- LOADING debug.rel
// ===========================================================================
//
// The DOL is fully table-driven. All three RELs are LZ blobs inside
// aaaa.dat, described by a 16-byte-per-entry file table at 0x800E8AA8
// ({lzParams, flag|decompSize, offset, compressedSize}):
//   +0x00 menus.rel {0000040B 401027E4 00000800 0005A818}
//   +0x10 game.rel  {0000040B 402220F8 0005B800 000F4450}
//   +0x20 debug.rel {0000040B 4005912C 00150000 000271C0}  <- referenced by NOTHING
// handleLoadingProcess (0x800097A0) is the boot state machine; state 7
// hands &table[0] to ARAMTransfer (0x800A70DC), state 8 OSLinks it and
// calls its prolog (module+0x34), state 0xA then parks until someone
// raises the "menu finished" flag. Overwriting the menus entry with
// debug's values (3 words; word 0 is identical) makes every menu-REL load
// pull debug.rel instead.
//
// WHY NO GLUE IS NEEDED (traced through the disassembly of the DOL's
// drawing dispatcher and debug.rel's own boot code, 2026-08-28):
//   * debug.rel's _prolog (0x806401A0) calls
//     setDrawingListHeadFunctions(0x806401DC), which writes that address
//     into slot +0 of all three drawing banks (0x803C7A24 + i*0x80).
//   * RunDrawScripts_with_stack_variables (0x800B0CB8), called from main
//     every frame, walks the current bank starting at slot +0 -- so
//     0x806401DC IS CALLED, once per frame, by the stock frame loop.
//   * 0x806401DC is a self-replacing initializer: it zeroes the current
//     node's scratch (+0x14/+0x16), resets the drawing lists
//     (0x800B0BE8), re-registers the DOL base draw fns (0x80009144), sets
//     the clear colour, and then overwrites ITS OWN node slot with the
//     debug menu's frame main (0x80640734). From the second frame on, the
//     debug menu runs in its place.
// So the stock path already brings the module up. The old glue called
// 0x806401DC by hand from inside handleLoadingProcess, where the "current
// node" is the LOADER's node -- that installed the debug menu over the
// loader itself and reset the lists mid-load, which is what the frame
// pump and the node-repair code were compensating for. Deleting the glue
// deletes the problem. Verified live 2026-08-28: module id 1 links, the
// bank head becomes 0x80640734 on the next frame, and the game runs at a
// steady 60 VI/s with no exceptions.
//
// The old "swap alone hangs on a turquoise screen" reading was wrong in
// the same way: a flat clear colour IS what debug.rel draws (its .data
// +0xA8) once its head fn has run. The menu was up the whole time -- it
// just draws no text.
//
// WHAT YOU GET: the top-level test-menu selector, 0x80640734.
//   * D-pad Up/Down move the selection (index in the bank node's +0x16)
//   * A launches the selected entry through the 14-entry jump table at
//     0x80670CF8: each entry calls insertGraphicDrawingFunction(script, 10)
//     and swaps the node fn to the sub-scene runner 0x806409DC
//   * B+Y held -> alternate mode 0x8063F620
//   * START signals "menu finished" to the loader, which tears debug.rel
//     down and boots game.rel -- expect a crash if pressed
// The input the selector reads is the DOL-polled menu-input struct at
// 0x803C77B8 (+0 held, +2 newly pressed, +4 held/repeat), so controls work
// with no extra plumbing.
//
// ===========================================================================
// PART 2 -- WHY THE SUITE HAS NO TEXT
// ===========================================================================
//
// Two separate holes, neither of them a missing asset:
//
// (a) The top-level selector draws NOTHING. 0x80640734 is pure input
//     handling -- move the index, launch the entry -- with no draw calls
//     at all. Nothing was stripped here; the devs simply drove it blind.
//     Verified by capturing the XFB out of emulated RAM: a flat clear
//     colour, no geometry, and no textures bound (a texture dump over a
//     whole boot produces zero files).
//
// (b) The suite's in-scene menus -- the MenuItem trees whose ASCII labels
//     sit in debug.rel's .rodata ("MAIN MENU", "BAT MENU", "SIM NUM",
//     "LENGTH", "FIRST POS", "VEL X" ...) -- call the DOL to draw
//     themselves, and THAT function was compiled out of the retail DOL:
//
//         80048bec: lwz r4,8(r3)    ; rows = state->rowsPerPage
//                   lwz r0,4(r3)    ; count = state->count
//                   cmpw/bge/mr     ; n = min(count, rows)
//                   mtctr r4
//                   cmpwi r4,0
//                   blelr
//         80048c0c: bdnz 80048c0c   ; <- the per-row loop, body GONE
//                   blr
//
//     An empty countdown loop over exactly the rows it would have drawn.
//     Its sibling debug helpers went the same way (0x80026130 is a bare
//     `blr`), while the menu's INPUT handler right next door (0x80048820,
//     0x3CC bytes) survived intact -- which is why those menus still
//     respond to the d-pad while showing nothing. Both are called ONLY
//     from debug.rel (checked against merged DOL+menus.rel and
//     DOL+game.rel images: zero call sites), so hooking the stub cannot
//     affect normal play.
//
// CAVEAT, measured live 2026-08-28: in rep_7A28 (entry 3) the struct exists
// but is never POPULATED -- count = 0 and items = NULL, while rowsPerPage = 16
// and wrap = 1 are real initialised values. Its call is also gated on
// currentDrawingItem->+0x34 bit 0x8, which nothing observed sets. So that
// menu stays empty even if the gate is forced; whatever fills items/count
// (rep_7BF0's equivalent is state->items = ((MenuItem**)(base+0x62C))[level])
// is not entered. The renderer below is verified against rep_7A28's real
// 0x8068157C table -- it draws the moment a scene populates one.
//
// The state struct fn_80048BEC receives, read off the surviving input
// handler at 0x80048820:
//     +0x00 u32   (unused here)
//     +0x04 s32   count        -- number of items
//     +0x08 s32   rowsPerPage  -- visible rows (0x10 in the suite)
//     +0x0C s32   scroll       -- index of the top visible row
//     +0x10 s32   index        -- selected item
//     +0x14 s32   wrap         -- selection wraps at the ends
//     +0x18 MenuItem* items    -- 0x2C bytes each; +0x04 = char* label,
//                                 a NULL label terminates the array
//
// TEXT IS DRAWN WITH THE GAME'S OWN ENGINE. The DOL loads the text banks
// and font pages during boot states 0-3 (ARAMTransfer of the table
// entries at 0x800E8AD8/AE8/AF8, then initTextRendering at 0x80010FA0) --
// all of that happens BEFORE any REL loads, so the font is resident under
// debug.rel too. The pass that renders the ScreenText pool is a DOL base
// draw script (fn_80009094 -> maybeProcessUIUpdates 0x80035168 ->
// DrawTextOnCondition 0x80010F2C), registered by fn_80009144 -- which
// debug.rel's own head fn calls. So the pool works here; we just fill
// blocks in it.
//
// WHY NOT Include/Rio/ScreenText.h: it compiles to ~470 gecko lines per
// hook (printf formatter, {tag} markup, word wrap), and this Dolphin
// build only has 3256 BYTES of code-list space for ALL enabled codes
// ("Too many GeckoCodes! ... only 3168 remain" in the log). The tiny
// writer below is the same block-filling recipe with the formatter
// dropped: plain literal strings, one block per line.

#include "Include/text/text_channel.h"
#include "Include/musyx/musyx.h"

// ---- claimed RAM (see ClaimedFreeMemory.h) --------------------------------
#define TEXT_SLOTS      17            // the suite's rowsPerPage is 0x10, so 16
                                      // rows + a cursor marker. Drawing FEWER
                                      // than rowsPerPage makes the cursor walk
                                      // off the bottom for the difference before
                                      // the game scrolls -- it scrolls on its
                                      // own page size, not on what we draw.
#define TEXT_MAXLEN     27
#define VALUE_COLUMN    13            // glyph column the value starts at
#define TEXT_FIRST      (30 - TEXT_SLOTS)
#define TEXT_BUF_ADDR   0x802EC504    // TEXT_SLOTS * (TEXT_MAXLEN+1) u16 = 952 B
#define g_menuState     VAR_ADDRESS(u32, 0x802EC8BC)  // captured fn_80048BEC arg

// ---- the pieces of debug.rel / the DOL this code watches ------------------
#define g_loadedModuleId    VAR_ADDRESS(u32, 0x8063EFC0)   // 1 = debug.rel
#define DRAW_BANK_0         0x803C7A24
#define g_currentBankIndex  VAR_ADDRESS(u16, 0x803CC1B0)
#define DEBUG_SELECTOR_FN   0x80640734

// ---- entry 6, the Sound Test's own menu (rep_02A8) -----------------------
// fn_1_A348 is the scene's whole top level: d-pad up/down move a cursor mod 5,
// A swaps the node fn for lbl_1_data_1C8C[cursor], B/START leave. It draws
// nothing, so the five items below are the overlay's job.
#define SND_TOP_FN         0x806493DC   // fn_1_A348 (.text 0xA348)
#define SND_LABELS         0x80672298   // 19 const char*; [0..4] are the items
#define SND_CURSOR         0x806E1B98   // lbl_1_common_bss_49A78, s8, 0..4
#define SND_ITEMS          5
// Items 0 and 1 open the two Voice Tests, which are the same loop twice over
// (fn_1_BFB0 / fn_1_BC00): left/right step a sound number by 1, Y+left/right by
// 10, A plays it, B goes back. The number is the whole scene, and nothing draws
// it -- label[5] ("VOICE NO ") is the caption the devs never wired up.
#define SND_VOICE_FN       0x8064B044   // fn_1_BFB0  (.text 0xBFB0)
#define SND_VOICE_NO       0x8069B176   // lbl_1_bss_3056, s16, wraps at 0xE9
#define SND_VOICE2_FN      0x8064AC94   // fn_1_BC00  (.text 0xBC00)
#define SND_VOICE2_NO      0x8069B174   // lbl_1_bss_3054, s16
#define SND_CAPTION        (SND_LABELS + 5 * 4)
// Item 2, Training Se Test (fn_1_A464): the same shape as the voice screens,
// but its sound number lives in .data rather than .bss.
#define SND_TRAIN_FN       0x806494F8   // fn_1_A464  (.text 0xA464)
#define SND_TRAIN_NO       0x806727E4   // lbl_1_data_1CA4, s16
// Item 3, SE Test (fn_1_B5B8): four sound slots and a row cursor. Y/X pick a
// mode and only mode 0 takes input; up/down move the row; left/right step that
// row's value; A plays it. The game names none of the four rows, so they are
// numbered here rather than invented.
#define SND_SE_FN          0x8064A64C   // fn_1_B5B8  (.text 0xB5B8)
#define SND_SE_MODE        0x8069B100   // lbl_1_bss_2FD8 +0x08, u8: Y->0, X->1
#define SND_SE_ROW         0x8069B101   // +0x09, u8 0..3
#define SND_SE_V0          0x806727E2   // lbl_1_data_FC0 +0xCE2, s16 (wraps 0x151..0x1B6)
#define SND_SE_V1          0x8069B102   // +0x0A
#define SND_SE_V2          0x8069B104   // +0x0C
#define SND_SE_V3          0x8069B106   // +0x0E
// Rows 1 and 2 do NOT play their value: they use it as an index into a table
// of real sound ids, so the number on screen means nothing on its own. Row 0
// plays its value directly; row 3 goes to a different API (fn_80062890).
#define SND_SE_TBL1        0x806723CC   // lbl_1_data_FC0+0x8CC, 48 ids (idx 0..0x2F)
#define SND_SE_TBL2        0x8067242C   // lbl_1_data_FC0+0x92C, 24 ids (idx 0..0x17)

// ---- entry 0, the Stadium Viewer's hidden picker (rep_0138) --------------
// fn_1_6848 installs fn_1_6DEC (0x806458DC) as the picker: d-pad up/down move a
// selection 0..6 and A commits it and loads that stadium. It draws nothing at
// all, which is the "invisible menu". A is one-way -- once the viewer
// (0x80645EA8) is up, B does not come back here.
// Index order is the game's own STADIUM_ID enum (Include/mssbTypes.h), verified
// one boot per index against captures of what actually loads.
#define STADIUM_PICK_FN    0x806458DC
#define STADIUM_SEL        0x806DF604   // .bss +0x474E4, u8 0..6, up = -1
#define STADIUM_COUNT      7

#define TXT_WHITE  0xFFFFFFFF
#define TXT_YELLOW 0xFFFF20FF
#define TXT_GRAY   0x909090FF

// ASCII -> MSSB glyph code (same table as ScreenText.h). Space and the
// terminator are control codes, not glyphs.
static u16 DebugGlyph(char c)
{
    if (c == ' ')             return 0x4002;
    if (c >= 'a' && c <= 'z') return c - 'a' + 62;
    if (c == ']')             return 59;
    if (c == '^')             return 60;
    if (c >= '!' && c <= '[') return c - '!';
    return 30;                /* '?' */
}

// Fill one pool block with a literal string. Slots are ours alone: nothing
// else uses the pool while debug.rel is the loaded module.
// Fill one pool block with a literal string, optionally followed by a value
// column. Slots are ours alone: nothing else uses the pool while debug.rel is
// the loaded module.
//
// valPtr != NULL draws the row's live value at a fixed column. The suite's
// MenuItem value rows carry it at +0x1C, with the row TYPE at +0x00:
// type 0 = plain integer (e.g. SIM NUM, range 1..100), type 1 = fixed point
// scaled by 100000, which is how the scenes read it back (rep_7BF0 divides
// every one of these by 100000.0f), so those print as n.nn.
static void DebugRow(int slot, int x, int y, u32 color, const char* s,
                     const s32* valPtr, s32 fixedPoint)
{
    u16* out = (u16*)(TEXT_BUF_ADDR + slot * ((TEXT_MAXLEN + 1) * 2));
    ScreenText* t = &screenTextArray.blocks[TEXT_FIRST + slot];
    u32* raw = (u32*)t;
    int n = 0;
    int i;

    while (*s != 0 && n < TEXT_MAXLEN)
        out[n++] = DebugGlyph(*s++);

    if (valPtr != 0)
    {
        s32 v = *valPtr;
        s32 whole = fixedPoint ? v / 100000 : v;
        s32 frac = fixedPoint ? v % 100000 : 0;
        u32 u;
        u16 tmp[12];
        int c = 0;

        while (n < VALUE_COLUMN && n < TEXT_MAXLEN)   /* pad to the value column */
            out[n++] = 0x4002;                        /* space */
        if (v < 0 && n < TEXT_MAXLEN)
            out[n++] = 12;                            /* '-' */
        if (whole < 0)
            whole = -whole;
        if (frac < 0)
            frac = -frac;
        u = (u32)whole;
        do
        {
            tmp[c++] = (u16)(15 + (u % 10));          /* '0'..'9' = 15..24 */
            u = u / 10;
        } while (u != 0 && c < 10);
        while (c > 0 && n < TEXT_MAXLEN)
            out[n++] = tmp[--c];
        if (fixedPoint)
        {
            u32 f = (u32)frac / 1000;                 /* two decimals */
            if (n < TEXT_MAXLEN)
                out[n++] = 13;                        /* '.' */
            if (n < TEXT_MAXLEN)
                out[n++] = (u16)(15 + ((f / 10) % 10));
            if (n < TEXT_MAXLEN)
                out[n++] = (u16)(15 + (f % 10));
        }
    }

    out[n] = 0x4000;                 /* end of string */

    for (i = 0; i < 14; i++)         /* 14 words = the whole block */
        raw[i] = 0;
    t->bankText             = (u16*)out;
    t->color                       = (s32)color;
    t->x                        = (u16)x;
    t->y                        = (u16)y;
    t->maxLettersToDraw        = -1;   /* -1 = draw the whole string */
    t->drawGroup                = 5;    /* draw group; 1-8 render every frame */
    t->style                  = 1;    /* small font */
    t->lineSpacing                = 2;
    t->justify = 0;    /* left */
    t->state                = 2;    /* state: active -- set last */
}

static void DebugLine(int slot, int x, int y, u32 color, const char* s)
{
    DebugRow(slot, x, y, color, s, 0, 0);
}

// Draw a NUL-separated, double-NUL-terminated list of lines, highlighting the
// selected one and putting a cursor beside it. One packed literal and one loop
// costs far less code-list space than a DebugLine call per row -- which matters,
// because Rio's gecko region is only about 8 KB INCLUDING the codehandler, and
// the list has to end up above the game's arena or it is silently clobbered.
static int DebugList(int slot, int x, int y, int step, const char* list, s32 sel)
{
    int i;

    for (i = 0; *list != 0; i++)
    {
        DebugRow(slot + i, x, y + i * step,
                 (i == sel) ? TXT_YELLOW : TXT_WHITE, list, 0, 0);
        while (*list != 0)
            list++;
        list++;                       /* step over the NUL to the next entry */
    }
    if (sel >= 0 && sel < i)
        DebugRow(slot + i, x - 16, y + sel * step, TXT_YELLOW, ">", 0, 0);
    return i + 1;                     /* slots consumed */
}

// A scene's own node is NOT the bank head -- the head runs the sub-scene runner
// (0x806409DC) and the scene sits further down the chain the frame loop walks
// (node +0x00 = fn, +0x08 = next). Walk it and hand back the node running `fn`,
// or 0. Bounded at 16 hops: the live list is five nodes deep.
static u32 FindSceneNode(u32 fn)
{
    u32 node = DRAW_BANK_0 + (u32)g_currentBankIndex * 0x80;
    int i;

    for (i = 0; i < 16; i++)
    {
        if (node < 0x80000000 || node >= 0x81800000)
            break;
        if (VAR_ADDRESS(u32, node) == fn)
            return node;
        node = VAR_ADDRESS(u32, node + 8);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// 1. The loader: point the menu-REL file-table entry at debug.rel's blob.
//    Idempotent, in place long before state 7 performs the first menu-REL
//    load, and still in place for any later menu-REL reload.
// ---------------------------------------------------------------------------
CGECKO(load_debug_rel_over_menus, .state = MSSB_ALWAYS);
void load_debug_rel_over_menus(void)
{
    VAR_ADDRESS(u32, 0x800E8AAC) = 0x4005912C; // flag | decompressed size
    VAR_ADDRESS(u32, 0x800E8AB0) = 0x00150000; // offset in aaaa.dat
    VAR_ADDRESS(u32, 0x800E8AB4) = 0x000271C0; // compressed size
}

// ---------------------------------------------------------------------------
// 2. Capture the stub's argument. Hooked at the stub's first instruction:
//    r0 is dead at a function entry, so cgecko's C2 wrapper (which does
//    mflr r0) is safe here, and the re-executed instruction is the stub's
//    own `lwz r4,8(r3)` -- it then runs its empty loop and returns as
//    before. Drawing happens in the per-frame code below, so this hook
//    stays one write long and costs almost no code-list space.
// ---------------------------------------------------------------------------
CGECKO(debug_menu_capture_state, .address = 0x80048BEC, .state = MSSB_ALWAYS,
                                 .instruction = "lwz 4, 8(3)");
void debug_menu_capture_state(void)
{
    READ_GAME_REG(u32, state, 3);
    g_menuState = state;
}

// ---------------------------------------------------------------------------
// 3. The text. Runs once per frame from the codehandler; the blocks it
//    fills are rendered by the DOL's text pass on the next frame.
//
//    Gated on the live bank-node function rather than on the module id
//    alone: before any REL loads, 0x8063EFC0 is plain heap and can
//    transiently read 1 during the intro, whereas 0x80640734 is only ever
//    installed by debug.rel's head fn.
//
//    Entry names are what live testing and the decomp's unit map
//    established; the two that only ever crash are listed by index so the
//    selection stays readable. Jump-table entries 10-13 are no-ops.
// ---------------------------------------------------------------------------
CGECKO(debug_menu_text, .state = MSSB_ALWAYS);
void debug_menu_text(void)
{
    u32 node, state;
    s32 sel, count, rows, scroll, i;
    u8* items;
    int slot;

    if (g_loadedModuleId != 1 || g_currentBankIndex > 2)
        return;
    node = DRAW_BANK_0 + (u32)g_currentBankIndex * 0x80;

    for (i = 0; i < TEXT_SLOTS; i++)          // release last frame's lines
        screenTextArray.blocks[TEXT_FIRST + i].state = 0;

    if (VAR_ADDRESS(u32, node) == DEBUG_SELECTOR_FN)
    {
        // ---- top-level selector ----
        sel = (s32)VAR_ADDRESS(s16, node + 0x16);
        DebugLine(0, 48, 26, TXT_YELLOW, "MSSB DEBUG MENU");
        // rep_0138 / rep_0250 / rep_0610 / rep_7A28 / rep_74A0 / rep_7730 /
        // rep_02A8 / rep_7730 / rep_7BA0 / rep_7BF0. Two carry a warning: entry
        // 1 runs but draws nothing, and entry 2's assets are not on the disc.
        DebugList(1, 56, 72, 28,
                  "0 Stadium Viewer\0"
                  "1 Countdown (invisible)\0"
                  "2 Char Viewer (no data)\0"
                  "3 Particle Editor\0"
                  "4 Model Viewer\0"
                  "5 Sprite Viewer\0"
                  "6 Sound Test\0"
                  "7 Sprite Unit\0"
                  "8 Cutscene Player\0"
                  "9 Bat Sim Menu\0", sel);
        DebugLine(12, 48, 380, TXT_GRAY, "D-PAD MOVE  A OPEN  B+Y ALT");
        return;
    }

    if (FindSceneNode(SND_TOP_FN))
    {
        // ---- entry 6: the Sound Test's five categories ----
        sel = (s32)(s8)VAR_ADDRESS(u8, SND_CURSOR);
        DebugLine(0, 48, 40, TXT_YELLOW, "SOUND TEST");
        for (i = 0; i < SND_ITEMS; i++)
        {
            const char* label = (const char*)VAR_ADDRESS(u32, SND_LABELS + i * 4);
            if ((u32)label < 0x80000000 || (u32)label >= 0x81800000)
                break;
            DebugLine(1 + i, 56, 96 + i * 28,
                      (i == sel) ? TXT_YELLOW : TXT_WHITE, label);
        }
        if (sel >= 0 && sel < SND_ITEMS)
            DebugLine(SND_ITEMS + 1, 40, 96 + sel * 28, TXT_YELLOW, ">");
        DebugLine(SND_ITEMS + 2, 48, 300, TXT_GRAY, "D-PAD MOVE  A OPEN  B BACK");
        return;
    }

    node = FindSceneNode(SND_VOICE_FN);
    if (node == 0)
        node = FindSceneNode(SND_VOICE2_FN);
    if (node)
    {
        // ---- entry 6, items 0/1: the two Voice Tests ----
        int two = (VAR_ADDRESS(u32, node) == SND_VOICE2_FN);
        const char* title = (const char*)VAR_ADDRESS(u32, SND_LABELS + (two ? 4 : 0));
        const char* cap   = (const char*)VAR_ADDRESS(u32, SND_CAPTION);
        s32 v = (s32)VAR_ADDRESS(s16, two ? SND_VOICE2_NO : SND_VOICE_NO);

        if ((u32)title >= 0x80000000 && (u32)title < 0x81800000)
            DebugLine(0, 48, 40, TXT_YELLOW, title);
        if ((u32)cap >= 0x80000000 && (u32)cap < 0x81800000)
            DebugRow(1, 56, 96, TXT_WHITE, cap, &v, 0);
        DebugLine(2, 48, 300, TXT_GRAY, "L/R PICK  HOLD Y+L/R X10");
        DebugLine(3, 48, 328, TXT_GRAY, "A PLAY  B BACK");
        return;
    }

    node = FindSceneNode(SND_TRAIN_FN);
    if (node)
    {
        // ---- entry 6, item 2: Training Se Test ----
        const char* title = (const char*)VAR_ADDRESS(u32, SND_LABELS + 2 * 4);
        s32 v = (s32)VAR_ADDRESS(s16, SND_TRAIN_NO);

        if ((u32)title >= 0x80000000 && (u32)title < 0x81800000)
            DebugLine(0, 48, 40, TXT_YELLOW, title);
        DebugRow(1, 56, 96, TXT_WHITE, "SE NO", &v, 0);
        DebugLine(2, 48, 300, TXT_GRAY, "L/R PICK  HOLD Y+L/R X10");
        DebugLine(3, 48, 328, TXT_GRAY, "A PLAY  B BACK");
        return;
    }

    node = FindSceneNode(SND_SE_FN);
    if (node)
    {
        // ---- entry 6, item 3: SE Test, four slots ----
        const char* title = (const char*)VAR_ADDRESS(u32, SND_LABELS + 3 * 4);
        s32 row  = (s32)VAR_ADDRESS(u8, SND_SE_ROW);
        s32 mode = (s32)VAR_ADDRESS(u8, SND_SE_MODE);
        s32 v[4];
        s32 id;

        v[0] = (s32)VAR_ADDRESS(s16, SND_SE_V0);
        v[1] = (s32)VAR_ADDRESS(s16, SND_SE_V1);
        v[2] = (s32)VAR_ADDRESS(s16, SND_SE_V2);
        v[3] = (s32)VAR_ADDRESS(s16, SND_SE_V3);
        if ((u32)title >= 0x80000000 && (u32)title < 0x81800000)
            DebugLine(0, 48, 40, TXT_YELLOW, title);
        {
            const char* nm = "SE 0 SE 1 SE 2 SE 3 ";
            for (i = 0; i < 4; i++)
            {
                DebugRow(1 + i, 56, 96 + i * 28,
                         (row == i) ? TXT_YELLOW : TXT_WHITE, nm, &v[i], 0);
                nm += 5;
            }
        }
        if (row >= 0 && row <= 3)
            DebugLine(5, 40, 96 + row * 28, TXT_YELLOW, ">");
        // rows 1/2 are indices, so show what they actually resolve to
        if (row == 1)
            id = (s32)VAR_ADDRESS(u16, SND_SE_TBL1 + v[1] * 2);
        else if (row == 2)
            id = (s32)VAR_ADDRESS(u16, SND_SE_TBL2 + v[2] * 2);
        else
            id = v[row & 3];
        DebugRow(6, 56, 224, TXT_WHITE, "SOUND ID", &id, 0);
        // X sets mode 1, and mode 1 makes the whole scene return before it
        // reads any button -- including B, so it looks like a lock-up.
        if (mode != 0)
            DebugLine(7, 48, 264, TXT_YELLOW, "FROZEN BY X - PRESS Y");
        DebugLine(8, 48, 300, TXT_GRAY, "UP/DN ROW   L/R VALUE");
        DebugLine(9, 48, 328, TXT_GRAY, "A PLAY   B BACK");
        return;
    }

    node = FindSceneNode(STADIUM_PICK_FN);
    if (node)
    {
        // ---- entry 0: the stadium picker ----
        sel = (s32)VAR_ADDRESS(u8, STADIUM_SEL);
        DebugLine(0, 48, 40, TXT_YELLOW, "STADIUM VIEWER");
        DebugList(1, 56, 82, 26,
                  "0 Mario Stadium\0"
                  "1 Bowser Castle\0"
                  "2 Wario Palace\0"
                  "3 Yoshi Park\0"
                  "4 Peach Garden\0"
                  "5 DK Jungle\0"
                  "6 Toy Field\0", sel);
        DebugLine(9, 48, 300, TXT_GRAY, "UP/DN PICK  A LOAD");
        return;
    }

    // ---- a scene's own menu, if its renderer ran this frame ----
    state = g_menuState;
    g_menuState = 0;                          // stale as soon as it stops drawing
    if (state < 0x80000000 || state >= 0x81800000)
        return;
    count  = (s32)VAR_ADDRESS(u32, state + 0x04);
    rows   = (s32)VAR_ADDRESS(u32, state + 0x08);
    scroll = (s32)VAR_ADDRESS(u32, state + 0x0C);
    sel    = (s32)VAR_ADDRESS(u32, state + 0x10);
    items  = (u8*)VAR_ADDRESS(u32, state + 0x18);
    if (count <= 0 || (u32)items < 0x80000000 || (u32)items >= 0x81800000)
        return;
    if (rows > TEXT_SLOTS - 1)
        rows = TEXT_SLOTS - 1;

    slot = 0;
    for (i = 0; i < rows; i++)
    {
        s32 idx = scroll + i;
        const char* label;

        if (idx < 0 || idx >= count)
            break;
        label = *(const char**)(items + idx * 0x2C + 4);
        if ((u32)label < 0x80000000 || (u32)label >= 0x81800000)
            break;                            // NULL label terminates the array
        {
            // +0x1C = pointer to the row's live value (0 on navigation rows);
            // +0x00 = 1 when that value is fixed point scaled by 100000.
            u32 vp = VAR_ADDRESS(u32, items + idx * 0x2C + 0x1C);
            s32 fixedPoint = (s32)VAR_ADDRESS(u32, items + idx * 0x2C + 0x00);
            if (vp < 0x80000000 || vp >= 0x81800000)
                vp = 0;
            DebugRow(slot, 56, 40 + i * 24,
                     (idx == sel) ? TXT_YELLOW : TXT_WHITE, label,
                     (const s32*)vp, fixedPoint);
        }
        if (idx == sel)
            DebugLine(TEXT_SLOTS - 1, 40, 40 + i * 24, TXT_YELLOW, ">");
        slot++;
    }
}

// ---------------------------------------------------------------------------
// 4. Silence the GX-thread OSPanic.
//
// The debug suite calls GX from a thread that is not the current GX thread, so
// two DOL texture setters trip a developer assert:
//
//     "sub.c: gOz_GXSetTexture was called in thread that is not current GX
//      thread."
//
// Both guards have the identical shape -- and the panic is the ONLY thing in
// the failure branch, so removing the call falls straight through to the same
// label the passing case branches to. That makes this exactly equivalent to
// Dolphin's "Ignore for this session", except it also holds on Dolphin builds
// where the panic is fatal instead of dismissible.
//
//     800241ac  bl GXGetCurrentGXThread      (gOz_GXSetTexture, sub.c:674)
//     800241b4  bl OSGetCurrentThread
//     800241bc  beq 800241d8                 <- passing case skips the panic
//     800241d4  bl OSPanic                   <- NOP this
//     800241d8  bl GXClearVtxDesc            <- both paths resume here
//
//     8002442c/80024434/8002443c/80024454/80024458   the same in
//                                  SetDisplayStateTexture (sub.c:574)
//
// Both are NOPed because the shared message names gOz_GXSetTexture either way
// (only the line number differs), so which one fired is not visible from the
// dialog. Inert in normal play: retail never calls GX off-thread, or every
// player would hit this.
// ---------------------------------------------------------------------------
ASM(nop_gx_thread_panic_a, "nop\n", .address = 0x800241D4, .state = MSSB_ALWAYS);
ASM(nop_gx_thread_panic_b, "nop\n", .address = 0x80024454, .state = MSSB_ALWAYS);

// ---------------------------------------------------------------------------
// 5. Stop entry 4 (Model Viewer) from taking a DSI on an unloaded skin.
//
// The viewer draws an actor whose geometry type byte says SKINNED (0x40) while
// its skin/bone array was never attached, so the DOL's actor draw hands SKNIt
// a NULL:
//
//     800b3b0c  lbz r0,6(r5)     ; geometry type
//     800b3b14  bne 800b3b2c     ; 0x40 -> skinned path
//     800b3b18  lwz r3,24(r25)   ; actor->+0x18   ok
//     800b3b1c  lwz r4,124(r25)  ; actor->+0x7C   NULL
//     800b3b24  bl  SKNIt        ; SKNIt entry does lbz r3,6(r4) -> DSI, DAR 0x6
//
// A NOP is not enough here (unlike the GX-thread panic) because the pointer is
// really used -- so this replaces the call with a guarded one: identical
// behaviour whenever the skin pointer is valid, and the draw is simply skipped
// when it is NULL. Only the case that currently crashes changes.
//
// r0 is dead at 0x800B3B24 (last written by the `lbz r0,6(r5)` that the cmplwi
// above already consumed), so cgecko's mflr-r0 C2 wrapper is safe here.
//
// SKNIt has one other caller, 0x800B2DA4, left alone -- it has not been seen
// taking a NULL. Add the same guard there if it ever does.
// ---------------------------------------------------------------------------
CGECKO(skn_null_guard, .address = 0x800B3B24, .state = MSSB_ALWAYS,
                       .instruction = "nop");
void skn_null_guard(void)
{
    // one READ_GAME_REG per function -- the macro declares its own _sp, so a
    // second use in the same scope is a redeclaration. Read the saved-register
    // frame directly for the rest.
    READ_GAME_REG(u32, actorGeo, 3);
    u32 skin  = *(volatile u32*)(_sp + 0x8 + 4);   /* r4 */
    u32 model = *(volatile u32*)(_sp + 0x8 + 8);   /* r5 */

    if (skin < 0x80000000 || skin >= 0x81800000)
        return;                      // skin never loaded: skip this draw
    ((void (*)(u32, u32, u32))0x800BF89C)(actorGeo, skin, model);
}

// ---------------------------------------------------------------------------
// 6. Make entry 1 usable: two independent faults, three word patches.
//
// fn_1_9AB0 (.text 0x9AB0 -> 0x80648B44) is entry 1's frame function, a
// three-phase state machine on the bank node's +0x10, with all its state in
// one .bss block (lbl_1_bss_2FA8 -> 0x8069B0C8, called `S` below):
//
//     phase 0  init:  seed S, S->0x1C = 0, phase = 1
//     phase 1  load:  if (S->0x1C > 0xB) { phase = 2; return; }
//                     if (diskReadRelated(&table[S->0x1C], S->0x1C)) S->0x1C++;
//     phase 2  run:   fn_1_97E4() reads input, then dispatch on S->0x02
//
// (a) THE LOOP RUNS ONE ENTRY PAST THE END OF ITS OWN TABLE.
//
// The descriptor table is lbl_1_data_AB0 + 0x45C (0x80671A4C), 16 bytes per
// entry in the same {lzParams, flag|decompSize, offset, compressedSize} shape
// as the DOL's REL table -- and it holds ELEVEN entries, not twelve. Dumped
// live:
//
//     [ 0] 0000040B 4005BDE4 0EAB5000 000161A0
//     ...
//     [10] 0000040B 4000785C 0EAE1800 000023A8
//     [11] 01000003 1B1A1918 17161514 13121110   <- the next data object
//
// So the twelfth iteration hands neighbouring data to ARAMTransfer as if it
// were a file descriptor. The buffer that comes back is not a resource, and
// diskReadRelated's relocation pass walks off it:
//
//     8000B9C8  lwz r0,8(r3) ; add r0,r0,r3 ; stw r0,8(r3)   r3 = 0x80775D40
//     8000B9EC  lwz r8,8(r3)                                 r8 = 0x00EEBA80
//     8000B9F0  lwz r5,0(r8)                                 <- DSI, CPU stops
//
// (the buffer's +0x8 already held 0x80775D40, i.e. its own address, so the
// unconditional `+= base` wrapped below 0x80000000). Measured live: viCount
// freezes the frame the twelfth load starts, phase stuck at 1, S->0x1C at 11.
// The fix is the loop bound, not the fault site: clamp the compare at
// .text 0x9B8C from 0xB to 0xA.
//
// The earlier reading of this crash -- "pressing A re-runs the load over an
// already-relocated buffer, so fn_8000B9C8's `+= base` doubles" -- was wrong.
// Nothing loads twice: the twelve slots in the DOL's resource table
// (0x803C4BE0, 0x3C stride) are all empty until the first A press and are then
// filled exactly once. That is also why both fixes built against that reading
// failed -- there is no second load to suppress.
//
// (b) THE SCENE'S GRAPHICS ELEMENT IS NEVER ASSIGNED.
//
// With the loop fixed the scene reaches phase 2, and then A kills it. A is
// handled at .text 0x98A8 (in fn_1_97E4): it cycles S->0x07 through 0..2 and
// then does `removeGraphicsElementFromScene(S->0x24)` -- and S->0x24 is zero,
// so the DOL takes a DSI on `lhz r7,0x14(r3)`. Guarding the DOL is not enough:
// the mode-0 body at .text 0x9C64 does remove/add on S->0x24 and then
// dereferences it again itself (`lhz r4,0x14(r4)` at .text 0x9C8C), so the
// pointer has to be real.
//
// It is meant to be the scene's own drawing node: BOTH sibling scene functions
// in this unit open with exactly that assignment --
//
//     fn_1_8F34 .text 0x8F80   stw r30, 0x24(r31)   r30 = currentDrawingItem
//     fn_1_9380 .text 0x93C4   stw r28, 0x24(r31)   r28 = currentDrawingItem
//
// -- and fn_1_9AB0 already holds currentDrawingItem in r29 from its own
// prologue. Its phase-0 init sets nine other fields of S but not this one.
// So the patch adds the missing store, using two words that are free because
// phase 0 reloads currentDrawingItem into r3 when r29 already has it:
//
//     .text 0x9B6C  lwz r3,0(r3)     -> stw r29,0x24(r31)   (the missing store)
//     .text 0x9B80  sth r0,0x10(r3)  -> sth r0,0x10(r29)    (r3 is now unused)
//
// The replacement halfword store is byte-identical to the one this same
// function already uses at .text 0x9B98, so it needed no guesswork.
//
// Verified live 2026-08-28: entry 1 loads, reaches phase 2, and survives A
// indefinitely at a steady 60 VI/s; addGraphicsElementToScene registers the
// scene's four sprites (node +0x14 = 5, +0x16 = 4, four new
// graphicsRelatedArray slots pointing back at the node) and its counter at
// lbl_1_data_AB0 + 0xD4 ticks down from 3000. So this is the scene running as
// written, not a suppressed fault. It still draws nothing on screen -- the same
// unsolved "invisible scene" state as entries 0, 3, 6 and 8.
//
// WHY THIS IS A PER-FRAME WRITE AND NOT AN 04 CODE: these are REL addresses, so
// a static write would also land in whatever module occupies that range
// otherwise. The MSSB_BOOT gate does match here (`rel` at 0x800E877C reads 0
// under debug.rel) but it equally matches real boot, when that range is
// unallocated arena or someone else's buffer. Writing from the per-frame code
// costs nothing on Project Rio and carries two conditions no gecko conditional
// can express: the loaded module really is debug.rel, and each target really
// still holds the instruction we expect.
//
// The writes are followed by DCFlushRange + ICInvalidateRange (both DOL
// functions, resident from boot) because this is self-modifying code: without
// them the stores sit in the data cache while the instruction fetch reads
// stale memory. They land many seconds before entry 1 can be selected, so the
// one-time cost is invisible.
// ---------------------------------------------------------------------------
#define DEBUG_TEXT_BASE     0x8063F094      // debug.rel .text, in the menu slot
#define ENTRY1_ELEM_STORE   (DEBUG_TEXT_BASE + 0x9B6C)
#define ENTRY1_PHASE_STORE  (DEBUG_TEXT_BASE + 0x9B80)
#define ENTRY1_LOOP_CMP     (DEBUG_TEXT_BASE + 0x9B8C)
#define DCFlushRange        ((void (*)(u32, u32))0x8006E894)
#define ICInvalidateRange   ((void (*)(u32, u32))0x8006E94C)

// Writes `fixed` only over the exact instruction it expects to replace, so a
// stray module id can never turn this into a blind poke. Returns 1 if it wrote.
static int PatchWord(u32 addr, u32 orig, u32 fixed)
{
    if (VAR_ADDRESS(u32, addr) != orig)
        return 0;                     // already patched, or not debug.rel's code
    VAR_ADDRESS(u32, addr) = fixed;
    return 1;
}

CGECKO(debug_rel_code_patches, .state = MSSB_ALWAYS);
void debug_rel_code_patches(void)
{
    int wrote;

    if (g_loadedModuleId != 1)
        return;
    wrote  = PatchWord(ENTRY1_ELEM_STORE,  0x80630000, 0x93BF0024);
    wrote += PatchWord(ENTRY1_PHASE_STORE, 0xB0030010, 0xB01D0010);
    wrote += PatchWord(ENTRY1_LOOP_CMP,    0x2C04000B, 0x2C04000A);
    if (wrote)
    {
        // one 64-byte range covers both cache lines the three words fall in
        DCFlushRange(ENTRY1_ELEM_STORE & ~31, 64);
        ICInvalidateRange(ENTRY1_ELEM_STORE & ~31, 64);
    }
}

// ---------------------------------------------------------------------------
// 7. removeGraphicsElementFromScene: return early for an element that owns
//    nothing. Without this, "remove" silently corrupts every OTHER element.
//
// The DOL keeps one flat table of 0x360 graphics slots (graphicsRelatedArray,
// 0x80371C30, 8 bytes per slot). A drawing node that owns sprites records its
// first slot at +0x14 and how many at +0x16. removeGraphicsElementFromScene
// (0x80034CEC) drops that run and compacts the table down over the hole,
// renumbering each surviving element as it goes:
//
//     80034D5C  lwz/stw 0(r5)->0(r3), 4(r5)->4(r3)   ; shift a slot down
//     80034D6C  lwz r4,4(r3)
//     80034D78  sth r6,0x14(r4)                      ; elem->0x14 = its new index
//     80034D84  addi r6,r6,1
//
// It never checks the count first. Called with +0x14 = 0 and +0x16 = 0 -- an
// element that owns NO slots -- r7 and r8 are both 0, so src == dst and the
// shift moves nothing, but the loop still runs all 0x360 iterations and stamps
// `r6` into +0x14 of every element it passes. Any element spanning several
// slots comes out with its LAST index in +0x14 instead of its first, so the
// next remove() on it drops the wrong run. Nothing else is visibly wrong at the
// time; the damage only surfaces later, in another scene.
//
// This is reached from two places, both of which do `remove(x)` before the
// matching `add(x)`: the mode-0 body of entry 1 (.text 0x9C64) and its B
// handler (.text 0x98F4). Retail never hits it because retail never removes an
// empty element -- which is why a one-instruction count check is inert outside
// the debug suite.
//
// Found by cycling entries: entry 1 alone and entry 4 alone each enter and exit
// cleanly eight times, but 4 -> 1 -> 4 -> 1 wedges the console on the second
// visit to entry 1. Its node has +0x14 = +0x16 = 0 (nothing ever called
// addGraphicsElementToScene on it), so B's remove() renumbered the whole table.
//
// Written as ASM rather than C because a C2 in C cannot return from the
// function it is injected into. cgecko emits an ASM hook verbatim, and a gecko
// C2 reaches its payload by a plain branch, so LR still holds the caller's
// return address and `beqlr` leaves removeGraphicsElementFromScene properly.
// r0 and CR0 are both dead at a function entry, so using them is safe.
// ---------------------------------------------------------------------------
ASM(gfx_remove_empty_guard,
    "lhz 0, 0x16(3)\n"        /* r3 = the element; +0x16 = slots owned */
    "cmplwi 0, 0\n"
    "beqlr\n",                /* owns nothing -> nothing to remove */
    .address = 0x80034CEC, .state = MSSB_ALWAYS,
    .instruction = "lis 4, 0x8037");   /* the overwritten graphicsRelatedArray@ha */

// ---------------------------------------------------------------------------
// ENTRY 2 (Character Model Viewer) -- DIAGNOSED, NOT FIXABLE.
//
// It loads its characters BY PATH, and those paths exist nowhere on a retail
// disc. fn_1_16978 (.text 0x16978 -> 0x80655A0C) is a 30-state loader driven by
// the node's +0x10 and dispatched through jumptable_1_data_F810 (0x80680350);
// twelve of its states call ARAMTransfer with a 16-byte descriptor out of
// lbl_1_data_2398 (0x80672ED8, nineteen per character). Unlike every retail
// descriptor, whose first word is the LZ parameter 0x0000040B, these lead with
// a POINTER to a name in debug.rel's own .rodata:
//
//     char/nin00/model0.dat   char/nin00/motb.dat   char/nin00/motr.dat ...
//
// debug.rel carries the full table -- nin00..nin53, 35 names each for the 32
// characters that had a complete motion set, 2 for the other 22. None of it is
// on the disc: the string "nin00" does not occur ANYWHERE in the retail image
// (searched all 1.4 GB -- not in ZZZZ.dat, not in aaaa.dat, not in the DOL).
// The shipped archive is offset-addressed only; its name directory was dropped
// when the game was built, so every one of these lookups resolves to offset 0.
//
// What that does, measured live: the loader reads ZZZZ.dat (DVD entry 22) from
// offset 0 with the descriptor's compressed size, the LZ decode yields nothing,
// and manageFileReadingProcess's stuck-detector fires --
//
//     800A6BBC  lbz r0,0x364(r31)   ; decoder error code -- ZERO here
//     800A6BC8  lbz r0,0x6d8(r30)   ; input exhausted     -- set
//     800A6BD4  lwz r0,0x6d4(r30)   ; bytes pending       -- zero
//     800A6BE0  DVDCancel + OSCancelThread x2 + OSCreateThread + OSResumeThread
//     800A6C54  OSReport("Decode Error Retry : EntryNo%d %x Err:%d")
//
// -- so every press that changes character tears down and rebuilds the DVD-read
// and decompressor threads. Six of those and the console wedges: an exception
// at fn_1_135C0 + 0xC (the `stfd f31,0x60(r1)` in its prologue) with MSR[FP]
// clear in SRR1, i.e. an FP-unavailable that never gets serviced. Identical
// SRR0/LR/SRR1 across runs. The first five failures are survivable, which is
// why entry 2 looks fine until it isn't.
//
// Two bypasses were built and REJECTED after live testing:
//   * forcing the loader's dispatch straight to its terminal state 29
//     (0x80655A3C `lha r6,0x10(r3)` -> `li r6,29`) stops every decode error,
//     but the scene then faults on the buffer pointers those loads used to
//     write into hugeAnimStruct+0xC0C;
//   * NOPing the install of the loader itself (0x80653FD4 `stw r0,0(r3)`) buys
//     one more round of input and then faults elsewhere.
// Each bypass just moves the fault, because the scene genuinely has no data to
// draw -- it renders a flat clear colour and nothing else, before or after.
// Entry 2 is a dead end on retail; it is left alone and labelled accordingly.
//
// STILL OPEN, and worth more than entry 2: **B kills entries 1, 4 and 9.**
// B is each scene's "leave and go back to the selector". All three die with the
// same signature -- LR = 0x800B0D08 (inside RunDrawScripts_with_stack_variables
// at 0x800B0CB8), and SRR0 = CTR = an instruction WORD rather than an address
// (0x2C000004, 0x3CA00000, 0x38C00004), with SRR1 bit 0x40000000 set: an ISI,
// i.e. the frame loop `bctrl`d into a drawing node whose function slot is not a
// function. One shared teardown bug rather than three. Entries 0, 2, 3, 5, 6,
// 7 and 8 survive B, so whatever it is, it is not universal.
// ---------------------------------------------------------------------------
