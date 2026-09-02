/*###########################################################
# Debug Mode From Main Menu
###########################################################*/
// Author: LittleCoaks
// *Replaces the main menu's "Records" button with launching the game's unused
//  developer DEBUG MENU (debug.rel). Minimal loader only: no text overlay, no
//  in-suite fixes -- see Gecko Codes/Global/Debug Mode.c for those.
//
// HOW IT WORKS
//
// The original code swapped the menu entry of the DOL's REL file table
// (0x800E8AA8, 16 bytes per entry: menus +0, game +0x10, debug +0x20) for
// debug's values at boot, so the very first menu-REL load pulled debug.rel:
//     040E8AAC 4005912C / 040E8AB0 00150000 / 040E8AB4 000271C0
// This code does the same swap, but from the Records button, and then makes
// the DOL loader RE-LOAD the menu slot instead of moving on to game.rel.
//
// The loader is handleLoadingProcess (0x800097A0), a state machine on the
// fixed node at 0x80111300 that RunDrawScripts calls first every frame:
//   +0x10  s16  "finished" flag a REL raises to hand control back
//   +0x14  ptr  the linked module
//   +0x18  u16  loader state
// While a REL runs the loader parks in state 0xA. When +0x10 becomes 1 it
// tears the REL down (maybeLoadsGameSoundFiles, resetAllDrawingStructs,
// fn_80009144, the REL's epilog, OSUnlink) and sets state 0xB, which loads
// game.rel. State 8 is the post-game "reload the menu slot" path (ARAMTransfer
// of table entry 0, then state 9 links it and calls its prolog) -- exactly the
// boot path the original code rode, minus the video-mode setup.
//
// So: on Records, swap the table, raise "finished" the way menus.rel itself
// does when it hands back (parent->+0x10 = 1 + removeCurrentDrawingItem, see
// menus.rel .text 0x1A3B4), skip the changeScreenVariables(8) call, and hook
// the one instruction that writes 0xB so it writes 8 instead.
//
// WHY THE MENU STAYED ON SCREEN (the earlier "press Y" attempt, and this code
// before the wipe below -- verified live 2026-09-01): the menu's 2D graphics
// are not drawn by menus.rel. They are element records in the DOL's own pool,
// menuGraphicsStructures (0x8039C3E0, 864 x 0xC0 bytes, +0x54 != 0 = in use),
// drawn every frame by the DOL base draw script maybeProcessUIUpdates
// (0x80035168) -- which debug.rel re-registers through fn_80009144. Neither
// the loader teardown nor the REL epilog frees them, and the main menu's exit
// fade (scene-manager mode 2, a full-screen black element) is one of them.
// At boot the pool is all zero, which is the state the boot-time load ran in,
// so this code zeroes the pool (and the ScreenText blocks, as the DOL's own
// exit tasks do with text_freeAllBlocks). WHEN matters: doing it in the
// teardown frame crashed (DSI in fn_800B27DC under maybeProcessUIUpdates),
// because the DOL scene manager is still the bank head for that frame and the
// element pass still walks the menu's records. Right after debug.rel's prolog
// (loader state 9, 0x80009BA4) the prolog has already made its own init the
// bank head, and that init resets every drawing struct before any draw pass
// runs -- nothing references the pool any more, and the wipe is safe (this
// is also the timing the live RAM experiment used). With that, debug.rel
// shows its flat clear colour and its scenes draw (entry 3's grid checked).
//
// Nothing restores the table afterwards, so START in the debug menu (which
// raises "finished" too) still tears debug.rel down into game.rel and
// crashes, same as with the boot-time version.

// ---- DOL loader node (lbl_80111300) ----------------------------------------
#define LOADER_FINISHED   VAR_ADDRESS(short,    0x80111310)
#define LOADER_STATE      VAR_ADDRESS(halfword, 0x80111318)
#define LOADER_STATE_LOAD_GAME    0xB
#define LOADER_STATE_RELOAD_MENU  0x8

// ---- REL file table, menu entry (0x800E8AA8): words 1..3 -------------------
#define RELTAB_MENU_SIZE   VAR_ADDRESS(word, 0x800E8AAC)   // flag | decompSize
#define RELTAB_MENU_OFFSET VAR_ADDRESS(word, 0x800E8AB0)
#define RELTAB_MENU_CSIZE  VAR_ADDRESS(word, 0x800E8AB4)
#define DEBUG_REL_SIZE     0x4005912C
#define DEBUG_REL_OFFSET   0x00150000
#define DEBUG_REL_CSIZE    0x000271C0

// Project Rio scene id (0 boot / 4 menus.rel / 5 game.rel) -- the halfword
// every MSSB_MENU / MSSB_GAME gate tests. Cleared when leaving the menu so no
// menu-gated code keeps writing into the slot debug.rel is about to occupy
// (it reads 0 under a boot-loaded debug.rel too).
#define RIO_SCENE_ID      VAR_ADDRESS(halfword, 0x800E877C)

#define removeCurrentDrawingItem  FUNCTION_ADDRESS(void, 0x800B0A14, void)
#define text_freeAllBlocks        FUNCTION_ADDRESS(void, 0x8000FE54, void)
#define game_memset               FUNCTION_ADDRESS(void, 0x8000540C, void*, int, unsigned int)

// The DOL's 2D element pool (menuGraphicsStructures): 864 records of 0xC0.
#define UI_ELEMENT_POOL           ((void*)0x8039C3E0)
#define UI_ELEMENT_POOL_SIZE      0x28800

// Claimed RAM: "a debug launch is in flight" latch. ARMED is set by the
// Records hook and turned into LOADING by the state-0xB redirect; LOADING is
// consumed by the post-prolog wipe. Both loader hooks are otherwise inert.
#define g_debugArm        VAR_ADDRESS(word, 0x802EC2B4)
#define DEBUG_ARMED       0x0DEB0601
#define DEBUG_LOADING     0x0DEB0602

// ---------------------------------------------------------------------------
// 1. Records button. The main-menu dispatch is `li r3,8; bl
//    changeScreenVariables` at 0x80641848/0x8064184C (menus.rel .text 0x27B4).
//    Hook the bl, drop it (.instruction = nop), and hand the menu back to the
//    loader instead.
// ---------------------------------------------------------------------------
CGECKO(records_launches_debug, .address = 0x8064184C, .state = MSSB_MENU,
                               .instruction = "nop");
void records_launches_debug(void)
{
    RELTAB_MENU_SIZE   = DEBUG_REL_SIZE;
    RELTAB_MENU_OFFSET = DEBUG_REL_OFFSET;
    RELTAB_MENU_CSIZE  = DEBUG_REL_CSIZE;

    g_debugArm      = DEBUG_ARMED;
    LOADER_FINISHED = 1;              // loader: tear menus.rel down next frame
    removeCurrentDrawingItem();       // and stop running the menu, as stock does
    RIO_SCENE_ID    = 0;              // (the reload path leaves it at 1 afterwards)
}

// ---------------------------------------------------------------------------
// 2. Loader: `sth r0, 0x18(r29)` at 0x80009C0C is the only place state 0xA
//    advances to 0xB (load game.rel). It runs right after the REL's epilog
//    and OSUnlink. Write the state ourselves: 8 (reload the menu slot, now
//    debug.rel) when a launch is armed, 0xB otherwise.
//    r0 is dead after this store (the function returns straight after), so
//    cgecko's r0 clobber is harmless here.
// ---------------------------------------------------------------------------
CGECKO(loader_reloads_menu_slot, .address = 0x80009C0C, .state = MSSB_ALWAYS,
                                 .instruction = "nop");
void loader_reloads_menu_slot(void)
{
    if (g_debugArm == DEBUG_ARMED)
    {
        g_debugArm   = DEBUG_LOADING;
        LOADER_STATE = LOADER_STATE_RELOAD_MENU;
    }
    else
        LOADER_STATE = LOADER_STATE_LOAD_GAME;
}

// ---------------------------------------------------------------------------
// 3. Loader state 9, the instruction after the freshly linked REL's prolog
//    `bctrl` (0x80009BA4 = `li r0, 0`, so r0 is dead and re-issued by
//    .instruction). If this link was our debug launch, put the DOL's 2D pool
//    and text blocks back to their boot state -- see the header for why here
//    and not in the teardown frame.
// ---------------------------------------------------------------------------
CGECKO(debug_rel_linked, .address = 0x80009BA4, .state = MSSB_ALWAYS,
                         .instruction = "li r0, 0");
void debug_rel_linked(void)
{
    if (g_debugArm != DEBUG_LOADING)
        return;
    g_debugArm = 0;
    game_memset(UI_ELEMENT_POOL, 0, UI_ELEMENT_POOL_SIZE);  // menu sprites + fade
    text_freeAllBlocks();                                    // menu text blocks
}
