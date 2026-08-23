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

// ---- claimed RAM (see ClaimedFreeMemory.h) --------------------------------
#define g_dmmLoading VAR_ADDRESS(u32, 0x802EC288) // a loader task is in flight
#define g_dmmTicks  VAR_ADDRESS(u32, 0x802EC28C)  // watchdog while loading
#define DMM_OWNER            0x802EC290           // fake owner node for the task
#define g_dmmLoadDone VAR_ADDRESS(u16, 0x802EC2A0)  // = DMM_OWNER + 0x10
#define g_dmmSavedVol VAR_ADDRESS(u32, 0x802EC2A8)  // 0 = nothing saved, else 0x100 | original

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

// Per-frame code (no .address), menu state only -- every address it touches is
// menu-side sound state.
CGECKO(DictionaryReplacesMenuMusic, .state = MSSB_MENU);
void DictionaryReplacesMenuMusic()
{
    u16 sc = *(u16*)(*(u32*)0x803CBBCC + 2);    // menuCtrl->screenCode
    u8* fx;
    u32 handle;

    // Off the main menu, leave everything alone -- including our own state. The
    // stock menu music survives a trip into a submenu untouched, so anything we
    // redo on the way back in would restart the track for no reason. The one
    // exception is handing the music volume back, which is just a u8 write.
    if (sc != 5)
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
    // means a submenu round trip is a no-op when nothing has changed.
    if (dmmFindFxEntry(DICT_MUSIC_FX) == 0)
    {
        // Not loaded. Never leave fx 484 pointing at a layer that cannot
        // resolve, or the next start returns -1 and the menu goes silent.
        fx = dmmFindFxEntry(MENU_MUSIC_FX);
        if (fx != 0 && *(u16*)(fx + 2) == DICT_MUSIC_LAYER)
            *(u16*)(fx + 2) = STOCK_MENU_LAYER;

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
