/*###########################################################
# Dictionary Replaces Menu Music
###########################################################*/
// Author: LittleCoaks
//
// *Plays the Dictionary scene's theme on the main menu instead of the normal main-menu theme.
//
// HOW THE MENU MUSIC ACTUALLY WORKS (reverse-engineered live 2026-07-24; the
// earlier "both songs are sfx 484 sharing one ARAM slot" theory was WRONG and
// could never work -- see the notes at the bottom).
//
// The menu BGM is started by the routine at 0x80062A94 as sndFXStartEx(484).
// An FX entry is 10 bytes: { u16 fxId; u16 objId; u8 prio, maxVoices, vol, pan,
// key, vel; }. The objId's top two bits are a TYPE TAG, not a flag:
// 0x0000 = macro, 0x4000 = keymap, 0x8000 = layer (see 0x800C54C8's
// `rlwinm r4,objId,0,16,17`). So:
//
//     fx 484 -> objId 0x8022 = LAYER 0x22  (menu theme,       group 32's pool)
//     fx   0 -> objId 0x8021 = LAYER 0x21  (Dictionary theme, group  0's pool)
//
// Group 32 is loaded on the menu; group 0 is NOT -- it only arrives when the
// Dictionary scene loads audio file set 4. Simply pointing fx 484 at layer 0x21
// from the menu makes sndFXStartEx return -1 (verified) and the menu goes
// silent, because the layer isn't in any loaded pool. So this code has to:
//
//   1. get audio file 4 loaded and its group 0 pushed, and only then
//   2. repoint fx 484 at layer 0x21 and restart the track.
//
// Step 1 does NOT drive the DVD by hand. Audio loading is a task node the game
// already knows how to run: task fn 0x80021758, with { +0x14 = push callback,
// +0x18 = state, +0x19 = audio file index }. It walks state 0 (start the load
// via 0x800A70DC) -> 1 (wait for the DVD, then relocate the descriptor) -> 2
// (call the push callback, flag the owner, deregister itself). The Dictionary's
// own node is pool entry 20, carrying exactly { 0x800627C4, state, id 4 } -- so
// we allocate the same shape and let the game do the work. 0x800627C4 is the
// game's own pushSoundGroup(0, *(0x800EF81C)) for that file set.
//
// Two traps worth knowing, both of which cost a debugging session:
//   - The FX table MOVES. Leaving a scene reloads the sound files to a new
//     address (seen: 0x80A84DF0 -> 0x80A862F0). Never cache the entry pointer;
//     re-find it through the loaded-group array every time.
//   - Do NOT restart the music with the fade u8 at 0x803C671A. That is the
//     stop path: it calls sndFXStop and then 0x800B0A14, which DEREGISTERS the
//     music updater, leaving the menu permanently silent. Restart by clearing
//     the "already playing" guard at 0x803C6718 instead.

#include "Include/game/UnknownHomes_Game.h"
#include "Include/musyx/musyx.h"

// ---- claimed RAM (see ClaimedFreeMemory.h) --------------------------------
#define g_dmmLoading VAR_ADDRESS(u32, 0x802EC288) // a loader task is in flight
#define g_dmmTicks  VAR_ADDRESS(u32, 0x802EC28C)  // watchdog while loading
#define DMM_OWNER            0x802EC290           // fake owner node for the task
#define g_dmmLoadDone VAR_ADDRESS(u16, 0x802EC2A0)  // = DMM_OWNER + 0x10
#define g_dmmSavedVol VAR_ADDRESS(u32, 0x802EC2A8)  // 0 = nothing saved, else 0x100 | original
#define g_dmmGone   VAR_ADDRESS(u32, 0x802EC2AC)  // consecutive frames fx 0 has been missing

// ---- game ------------------------------------------------------------------
#define insertTask  ((void* (*)(void*, u32))0x800B0A5C)
#define sndFXStop   ((void  (*)(u32))0x800C832C)

#define LOADER_TASK_FN      ((void*)0x80021758)  // audio-file loader state machine
#define PUSH_DICT_GROUP_FN  ((void*)0x800627C4)  // pushSoundGroup(0, *(0x800EF81C))
#define DICT_AUDIO_FILE     4                    // 0x800EF508[4]
#define MENU_MUSIC_FX       484
#define DICT_MUSIC_FX       0                    // group 0's only entry; marks it loaded
#define DICT_MUSIC_LAYER    0x8021               // 0x8000 = "layer", id 0x21
#define STOCK_MENU_LAYER    0x8022

#define fxGroupCount  VAR_ADDRESS(u16, 0x803CC2D8)
#define FX_GROUP_ARRAY             0x80316D70    // { u16 groupId; u16 numFx; u32; u32 fxTable; }
#define g_menuMusicHandle VAR_ADDRESS(u32, 0x803C6714)
#define g_menuMusicGuard  VAR_ADDRESS(u8,  0x803C6718)
#define g_menuMusicVol    VAR_ADDRESS(u8,  0x803CB888)  // volume the routine starts at

#define DMM_LOAD_TIMEOUT 900                     // ~15s; give up rather than hang silent

// The audio-file table slot the loader writes its buffer into: state 0 stores
// it at 0x800EF808 + fileIndex*4 + 4, so file 4 lands at 0x800EF81C -- which
// is exactly the word PUSH_DICT_GROUP_FN re-reads when it finally pushes.
#define g_dmmDesc VAR_ADDRESS(u32, 0x800EF81C)
#define pushDictGroup ((void (*)(void))PUSH_DICT_GROUP_FN)

// A loaded audio file is a 0x20-byte header of four { u32 off; u32 size; }
// section records -- prj, sdir, pool, samples -- followed by the sections, so
// the first one always begins 0x20 past the header.
#define DMM_DESC_HDR 0x20

// How long fx 0 has to stay missing before we believe it is gone. See
// dmmGroupReallyGone.
#define DMM_SETTLE_FRAMES 30

// What file 4's descriptor slot currently holds.
enum {
    DMM_DESC_NONE = 0,   // nothing loaded into this slot yet
    DMM_DESC_RAW,        // loaded, not yet relocated -- a task is mid-flight
    DMM_DESC_READY,      // loaded and relocated exactly once: safe to push
    DMM_DESC_BAD         // relocated more than once: pushing it would fault
};

// Whether this code should be doing anything right now is CGecko's
// CGECKO_ACTIVE (Common.h): 1 on its own, which is the whole point of the code.
// Inside RioModPack it is one selectable track among many, so the pack defines
// CGECKO_ACTIVE to "the menu music slot is set to Dictionary" before including
// the file, and the mod switches itself off when it is not.
//
// It has to SELF-gate rather than be wrapped in CGECKO_GATE_ADDR: it edits game
// state (fx 484's layer id, and the menu music volume u8), so it must keep
// running while off in order to put those back. A gate-wrapped code is simply
// not executed, and could never undo itself -- the same reason Duplicate
// Characters self-gates.

// Which menu screens this does its work on. screenCode 5 is the main menu, and
// on its own that is the only screen that matters -- the swap is for the main
// menu theme and nothing else changes it.
//
// A pack that lets the user CHOOSE this track has to add the screen that choice
// is made on, or the swap does not happen until they back out to the main menu.
// Every other track in RioModPack's music config is a stream and swaps the
// instant it is selected, so a Dictionary that waits reads as broken rather
// than as delayed -- which is exactly how it was reported.
//
// Widening this is safe because the work is idempotent: once fx 484 holds the
// Dictionary layer, a pass only re-finds the entry and re-pins the volume, and
// the restart is guarded on the layer having actually changed. Not widened to
// every menu screen, though: an inactive screen would also start an audio-file
// LOAD there, and pushing group 0 on a screen with its own audio set is not
// something any of this has been tested against.
#ifndef DMM_SCREENS
#define DMM_SCREENS(sc) ((sc) == 5)
#endif

// The loaded-group array is rebuilt (and the sound data relocated) on every
// scene change, so the entry has to be located fresh each time it's touched.
static u8* dmmFindFxEntry(u16 fxId)
{
    u16 groups = fxGroupCount;
    u16 i, j;

    for (i = 0; i < groups; i++)
    {
        u8* g     = (u8*)(FX_GROUP_ARRAY + i * 12);
        u16 count = *(u16*)(g + 2);
        u8* table = *(u8**)(g + 8);

        if (table == 0 || count > 200)          // sanity: array is live game state
            continue;

        for (j = 0; j < count; j++)
        {
            u8* entry = table + j * 10;
            if (*(u16*)entry == fxId)
                return entry;
        }
    }
    return 0;
}

// Classify the descriptor sitting in file 4's slot.
//
// WHY THIS EXISTS -- the crash it fixes. The loader task's state 1 relocates
// the descriptor IN PLACE and UNCONDITIONALLY:
//
//     d = *(0x800EF81C);
//     *(d+0x00) += d;  *(d+0x08) += d;  *(d+0x10) += d;  *(d+0x18) += d;
//
// turning the four section OFFSETS into absolute pointers. Nothing marks the
// descriptor as done, so running the task a second time for a file that never
// left RAM adds the base again and every section pointer becomes 2*d + off.
// State 2's push callback then RE-READS 0x800EF81C -- it does not reuse what
// state 1 saw -- and hands that to sndPushGroup, whose first act is
//
//     while (g->nextOff != 0xFFFFFFFF)      // g = prj_data
//
// a read straight through the wild pointer. Measured live from the crash
// savestate: d = 0x8110F1A0, prj went 0x20 -> 0x8110F1C0 (correct) ->
// 0x0221E360 (= 2*d + 0x20), and the game died with "Invalid read from
// 0x0221e360, PC = 0x800d3440" -- 0x800d3440 being inside sndPushGroup
// (0x800D3120 + 0x320). The other three sections were biased identically.
//
// The three interesting values of *(d+0) are all distinguishable:
//     0x20        -> loaded, relocation still to come
//     d + 0x20    -> relocated exactly once, which is what we want
//     2*d + 0x20  -> relocated twice, already ruined
static int dmmDescriptorState(void)
{
    u32 d = g_dmmDesc;
    u32 prj;

    if (d < 0x80000000 || d >= 0x81800000)
        return DMM_DESC_NONE;           // never loaded this session

    prj = *(u32*)d;
    if (prj == DMM_DESC_HDR)
        return DMM_DESC_RAW;
    if (prj == d + DMM_DESC_HDR)
        return DMM_DESC_READY;
    return DMM_DESC_BAD;
}

// Has group 0 really gone, or is the loaded-group array just mid-rebuild?
//
// dmmFindFxEntry walks live game state that a screen change tears down and
// puts back, so one missing frame means nothing. Acting on one is what queued
// a redundant load for an already-resident file and produced the crash above.
// Require the entry to stay missing for DMM_SETTLE_FRAMES; any sighting
// resets the count.
static int dmmGroupReallyGone(void)
{
    if (dmmFindFxEntry(DICT_MUSIC_FX) != 0)
    {
        g_dmmGone = 0;
        return 0;
    }
    if (g_dmmGone < DMM_SETTLE_FRAMES)
    {
        g_dmmGone++;
        return 0;
    }
    return 1;
}

// Stop the current track and let 0x80062A94 start it again next frame. It
// re-reads the FX entry on every start, so the new layer takes effect there.
static void dmmRestartMusic(void)
{
    u32 handle = g_menuMusicHandle;

    if (handle != 0 && handle != 0xFFFFFFFF)
        sndFXStop(handle);

    g_menuMusicHandle = 0;
    g_menuMusicGuard  = 0;                      // 0 = "not playing" -> routine restarts it
}

// The menu music routine starts the track at the MENU's music volume
// (`lbz r4,-32440(r13)` @0x80062AD0 -- 105 on a stock boot), NOT the FX entry's
// own default of 127. The entry default only applies when the caller passes
// 255, which this routine never does, so the Dictionary track ends up ~83% as
// loud here as it is in the Dictionary scene. Fix it at the source by pinning
// that u8 to the volume the sound was authored at.
//
// Only two instructions in the whole game read this u8 (0x80062AD0 for the
// start volume and 0x80062B3C as the fade base), both inside the music routine,
// so raising it cannot affect anything else. It does have to be re-applied every
// frame: something outside those readers writes it back (measured live -- it
// reverted 127 -> 105 on its own between two tests).
static void dmmPinVolume(u8* fx)
{
    if (g_dmmSavedVol == 0)
        g_dmmSavedVol = 0x100 | g_menuMusicVol;  // remember the stock value once

    g_menuMusicVol = *(fx + 6);                  // +6 = the entry's authored volume
}

// Put everything back: fx 484 pointed at the stock layer again and the menu
// music volume handed back. Used when CGECKO_ACTIVE goes false, and safe to call
// when nothing was ever changed -- both halves check first.
//
// The loaded sound group is deliberately NOT unloaded. It arrived through the
// game's own loader task and costs only pool space; dropping it would mean
// undoing a push the game normally owns, and re-selecting the track would then
// have to wait through the whole DVD load again.
static void dmmStandDown(void)
{
    u8* fx = dmmFindFxEntry(MENU_MUSIC_FX);

    if (fx != 0 && *(u16*)(fx + 2) == DICT_MUSIC_LAYER)
    {
        *(u16*)(fx + 2) = STOCK_MENU_LAYER;
        if (g_menuMusicHandle != 0 && g_menuMusicHandle != 0xFFFFFFFF)
            dmmRestartMusic();                  // swap back audibly, not on next scene
    }
    if (g_dmmSavedVol != 0)
    {
        g_menuMusicVol = (u8)(g_dmmSavedVol & 0xFF);
        g_dmmSavedVol  = 0;
    }
    g_dmmLoading = 0;
    g_dmmGone    = 0;
}

// Per-frame code (no .address), menu state only -- every address it touches is
// menu-side sound state.
CGECKO(DictionaryReplacesMenuMusic, .state = MSSB_MENU);
void DictionaryReplacesMenuMusic()
{
    u16 sc = *(u16*)(*(u32*)0x803CBBCC + 2);    // menuCtrl->screenCode
    u8* fx;
    u32 handle;

    if (!(CGECKO_ACTIVE))
    {
        dmmStandDown();
        return;
    }

    // Off the screens this runs on, leave everything alone -- including our own
    // state. The stock menu music survives a trip into a submenu untouched, so
    // anything we redo on the way back in would restart the track for no
    // reason. The one exception is handing the music volume back, which is just
    // a u8 write.
    if (!DMM_SCREENS(sc))
    {
        if (g_dmmSavedVol != 0)
        {
            g_menuMusicVol = (u8)(g_dmmSavedVol & 0xFF);
            g_dmmSavedVol  = 0;
        }
        return;
    }

    // Is the Dictionary's sound group still pushed? Its fx 0 is the marker.
    // Driving off what is actually loaded (rather than a linear state machine)
    // means a submenu round trip is a no-op when nothing has changed -- but the
    // probe has to be debounced, because the array it reads is rebuilt on a
    // screen change and reads empty for a few frames either side of one.
    if (dmmGroupReallyGone())
    {
        int desc = dmmDescriptorState();

        // Not loaded. Never leave fx 484 pointing at a layer that cannot
        // resolve, or the next start returns -1 and the menu goes silent.
        fx = dmmFindFxEntry(MENU_MUSIC_FX);
        if (fx != 0 && *(u16*)(fx + 2) == DICT_MUSIC_LAYER)
            *(u16*)(fx + 2) = STOCK_MENU_LAYER;

        // The file is still resident with its pointers already relocated --
        // the group was popped, but nothing needs re-reading from the disc.
        // Push it straight back. Handing this to the loader task instead is
        // what relocates the descriptor a second time and crashes the game;
        // see dmmDescriptorState.
        if (desc == DMM_DESC_READY)
        {
            pushDictGroup();
            g_dmmLoading = 0;
            g_dmmGone    = 0;
            return;
        }

        // A load is already in flight and has not reached its relocate step;
        // let it finish rather than stacking a second one on top.
        if (desc == DMM_DESC_RAW)
            return;

        // Already double-relocated by something else. Pushing it would fault,
        // and re-loading would not repair it (state 0 hands back the same
        // resident buffer). Stay off it and leave the stock theme playing.
        if (desc == DMM_DESC_BAD)
        {
            g_dmmLoading = 0;
            return;
        }

        if (g_dmmLoading == 0)
        {
            // Hand the game a loader task for the Dictionary's audio file set.
            // insertTask zeroes +0x14/+0x18 and sets +0x0C to the current node;
            // we repoint +0x0C at our own scratch so the "done" write lands
            // somewhere harmless and doubles as the completion flag.
            u8* node = (u8*)insertTask(LOADER_TASK_FN, 1);
            if (node == 0)
                return;                         // pool full, try again next frame

            g_dmmLoadDone = 0;
            *(u32*)(node + 0x0C) = DMM_OWNER;
            *(u32*)(node + 0x14) = (u32)PUSH_DICT_GROUP_FN;
            *(node + 0x18)       = 0;           // state 0 = begin the load
            *(node + 0x19)       = DICT_AUDIO_FILE;

            g_dmmTicks   = 0;
            g_dmmLoading = 1;
        }
        else if (++g_dmmTicks > DMM_LOAD_TIMEOUT)
        {
            g_dmmTicks   = 0;                   // let a later menu visit retry
            g_dmmLoading = 0;
        }
        return;
    }

    // Still inside the settle window: fx 0 was not found this frame, we just
    // do not believe it yet. Do nothing at all until it resolves one way or
    // the other -- pointing fx 484 at the Dictionary layer below while the
    // group really is absent makes the next sndFXStartEx return -1 and the
    // menu goes silent for the rest of the window.
    if (g_dmmGone != 0)
        return;

    // NB: do NOT clear g_dmmGone here. dmmGroupReallyGone already zeroes it on
    // a sighting, and clearing it again would also hit the settle frames it had
    // just incremented, holding the counter at zero so the debounce never fires.
    g_dmmLoading = 0;                           // loaded; the task has finished

    fx = dmmFindFxEntry(MENU_MUSIC_FX);
    if (fx == 0)
        return;

    // Pin the volume before any restart, so the start below reads the raised
    // value rather than the stock one.
    dmmPinVolume(fx);

    if (*(u16*)(fx + 2) != DICT_MUSIC_LAYER)
    {
        // A stock (or freshly reloaded) copy of the table -- point it at the
        // Dictionary track. Only interrupt playback if something is actually
        // playing, which can only be the stock theme; if nothing is, the game's
        // own next start reads the patched entry and needs no restart from us.
        *(u16*)(fx + 2) = DICT_MUSIC_LAYER;

        handle = g_menuMusicHandle;
        if (handle != 0 && handle != 0xFFFFFFFF)
            dmmRestartMusic();
    }
}
