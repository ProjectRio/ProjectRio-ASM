/*###########################################################
# Menu Music Track Select
###########################################################*/
// Author: LittleCoaks
//
// - Replaces the main-menu theme with one of three families of soundtrack,
// selected at build time by MMT_TRACK below:
//     * one of the game's streamed (.adp) soundtracks -- the stadium themes,
//       results, replay, victory, the staff roll, plus the two tracks that ship
//       on the disc but the stock game can never play;
//     * the stock main-menu theme, left untouched (MMT_DEFAULT); or
//     * the Dictionary scene's own theme (MMT_DICTIONARY), swapped in via the
//       Musyx layer trick from "Dictionary Replaces Menu Music".
// - A streamed track keeps playing across every menu screen -- it does not
// restart when you move between them -- and stops when the game REL loads for a
// match. It is muted on the Dictionary scene (screenCode 7) so its own
// soundtrack is heard.
//
// Deliberately declared with NO `.state`, so it runs in every state. A
// .state = MSSB_MENU code stops being executed the moment the game REL loads,
// which would leave the stream running into the match with nothing left to shut
// it off. (The default and dictionary options do not stream anything, so they
// simply idle outside the menu -- see their branches below.)
//
// ---------------------------------------------------------------------------
//  PICK YOUR TRACK -- set MMT_TRACK below
//  (named MMT_TRACK, not TRACK: GlobalData.h already declares `struct TRACK`)
// ---------------------------------------------------------------------------
//   -- MENU THEMES (not streams -- a different audio path entirely) --
//    0  default      stock main-menu theme, left alone (no override)
//    1  dictionary   the Dictionary scene's theme (Musyx layer swap; see
//                    "Dictionary Replaces Menu Music.c" for the full writeup)
//
//   -- STREAMED (.adp) SOUNDTRACKS --
//    2  mario_01     Mario Stadium       10  cha_victry   victory
//    3  koopa        Bowser Castle       11  toy          Toy Field
//    4  wario        Wario Palace        12  cha_map      Challenge Mode map
//    5  yoshi        Yoshi Park          13  cha_demo     demo  (see note)
//    6  peach        Peach Garden        14  cha_end_jin  ending jingle (short)
//    7  donkey       DK Jungle           15  cha_s_roll   staff roll / credits
//    8  replay       replay              16  home_in      home-run jingle (short)
//    9  result       results
//
//   -- UNUSED: present on the disc, unreachable by the stock game --
//   17  star_01_h.adp
//   18  star_03_h.adp
//
//   -- CUSTOM: tracks you add to the disc yourself --
//   19..28  snd/my_snd_h/custom_01_h.adp .. snd/my_snd_h/custom_10_h.adp
//
// A custom track needs no code change beyond the number above: the stream
// descriptor's path is resolved through the disc FST at runtime, so any file
// present in a rebuilt ISO can be opened. Encode it in the same format as the
// stock .adp files, drop it in snd/my_snd_h/ under one of the ten names -- note
// the `_h` suffix, which every streamed track on the disc carries -- and
// rebuild. If the selected file is not on the disc the code stands down and
// leaves the stock menu theme playing, so a build pointed at custom_04 still
// behaves on an unmodified ISO.
//
// Note on 13 (cha_demo): its volume u8 in the per-stream parameter table at
// 0x800E88A4 is 0x00 where every other track is 0x7F, so it will very likely
// play silent. Left selectable for completeness.
// Note on 14/16: these are short jingles, not looping music.
//
#define MMT_DEFAULT    0   // stock main-menu theme -- no override at all
#define MMT_DICTIONARY 1   // the Dictionary scene's theme (Musyx layer swap)

// Streamed tracks are 2..28; the game's own stream id is MMT_TRACK - 2.
#define MMT_STREAM_ID (MMT_TRACK - 2)

// Custom tracks: MMT_TRACK 19..28 -> snd/my_snd_h/custom_01.adp .. custom_10.adp
#define MMT_CUSTOM_FIRST  19
#define MMT_CUSTOM_COUNT  10
#define MMT_CUSTOM_NUMBER (MMT_TRACK - MMT_CUSTOM_FIRST + 1)   // 1..10

#define MMT_TRACK 19

#if MMT_TRACK > (MMT_CUSTOM_FIRST + MMT_CUSTOM_COUNT - 1)
#error "MMT_TRACK is out of range -- see the track list above"
#endif
//
// Which body compiles is chosen by the #if dispatch below. The stream path and
// the dictionary path both take over fx 484 in incompatible ways, so only one is
// ever built into the code at a time -- there is no runtime branching between
// them.

#include "Include/game/UnknownHomes_Game.h"

#include "Include/Local/Legacy.h"
// One code, declared once: every #if branch below defines the same
// MenuMusicTrackSelect() body, and with no .address it runs once per frame (and
// with no .state, in every game state -- see the note above).
CGECKO(MenuMusicTrackSelect);

// ===========================================================================
#if MMT_TRACK == MMT_DEFAULT
// ===========================================================================
// Nothing to do: leave the stock menu-music routine (0x80062A94 -> fx 484)
// running exactly as it does on a stock boot. Present as a build option so the
// track can be dialed back to default without disabling the code.

void MenuMusicTrackSelect()
{
}

// ===========================================================================
#elif MMT_TRACK == MMT_DICTIONARY
// ===========================================================================
// Play the Dictionary scene's theme on the main menu instead of the normal
// main-menu theme. This is the same technique as "Dictionary Replaces Menu
// Music.c" -- see that file for the full reverse-engineering notes. In brief:
// the menu BGM is sndFXStartEx(484), whose objId is a layer tag; the Dictionary
// theme is the same fx pointed at layer 0x21 instead of 0x22. That layer lives
// in group 0, which is only loaded when the Dictionary's audio file set 4 is
// pushed, so this has to (1) run the game's own audio-loader task to push it,
// then (2) repoint fx 484 at layer 0x21 and restart the track.
//
// Because this option streams nothing, it needs no cleanup when the game REL
// loads -- the game stops fx 484 for the match on its own. It simply idles
// outside the main menu (handing the raised music volume back on the way out).
//
// Uses its own claimed-RAM block (0x802EC2E0..) so it never collides with the
// standalone "Dictionary Replaces Menu Music" code's block (0x802EC288..).

// ---- claimed RAM (see ClaimedFreeMemory.h) --------------------------------
#define g_dmmLoading  VAR_ADDRESS(u32, 0x802EC2E0)  // a loader task is in flight
#define g_dmmTicks    VAR_ADDRESS(u32, 0x802EC2E4)  // watchdog while loading
#define g_dmmSavedVol VAR_ADDRESS(u32, 0x802EC2E8)  // 0 = nothing saved, else 0x100 | original
#define DMM_OWNER              0x802EC2F0            // fake owner node for the task
#define g_dmmLoadDone VAR_ADDRESS(u16, 0x802EC300)  // = DMM_OWNER + 0x10

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

// The menu music routine starts the track at the MENU's music volume (105 on a
// stock boot), NOT the FX entry's own default of 127, so the Dictionary track
// would play ~83% as loud here as it is in the Dictionary scene. Pin that u8
// to the volume the sound was authored at. It has to be re-applied every frame:
// something outside the two readers writes it back.
static void dmmPinVolume(u8* fx)
{
    if (g_dmmSavedVol == 0)
        g_dmmSavedVol = 0x100 | g_menuMusicVol;  // remember the stock value once

    g_menuMusicVol = *(fx + 6);                  // +6 = the entry's authored volume
}

// Hand the stock menu-music volume back once, if we raised it.
static void dmmRestoreVolume(void)
{
    if (g_dmmSavedVol != 0)
    {
        g_menuMusicVol = (u8)(g_dmmSavedVol & 0xFF);
        g_dmmSavedVol  = 0;
    }
}

void MenuMusicTrackSelect()
{
    u16 sc;
    u8* fx;
    u32 handle;

    // No `State:` line means this runs in every state; outside the menu REL the
    // screenCode pointer below is not valid, so bail early there.
    if (inningSetting.rel != 4)
    {
        dmmRestoreVolume();
        return;
    }

    sc = *(u16*)(*(u32*)0x803CBBCC + 2);        // menuCtrl->screenCode

    // Off the main menu (submenus), leave everything alone -- including our own
    // state. The stock menu music survives a trip into a submenu untouched, so
    // anything we redid on the way back in would restart the track for no reason.
    if (sc != 5)
    {
        dmmRestoreVolume();
        return;
    }

    // Is the Dictionary's sound group still pushed? Its fx 0 is the marker.
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

// ===========================================================================
#else   // a streamed (.adp) track
// ===========================================================================
// This is a completely different audio path from the dictionary option above.
// That one swaps a Musyx FX entry's layer id; this one drives the game's
// streaming JUKEBOX, which is where the actual soundtrack lives.
//
// `playStream(id)` @0x8006877C is the whole API -- one u8 in, everything else
// table driven:
//     - gated on enableMusic @0x800EFBA0 (reads 1, same u8 the Musyx music checks)
//     - per-stream {volume, pan} from 0x800E88A4[id*2]
//     - stream descriptor from 0x800E87B4[id], 16 bytes: {char* path, u32 size,
//       u32, u32 size}
//     - work buffer 0x8034E478 + id*80
//     - id 12 starts in one-shot mode, every other id loops
//
// The descriptor's path is resolved at RUNTIME through the disc FST
// (0x800A7544 does `DVDConvertPathToEntrynum(entry->path)`, then 0x800A750C does
// `DVDFastOpen`). Nothing about the filename is baked into the game: point a
// spare descriptor at a path string we build ourselves and the stock loader
// opens it happily. That is what makes the two unused tracks reachable -- they
// ARE in the FST, they simply have no descriptor and no path string anywhere in
// main.dol -- and it is equally what makes a file the user added to the disc
// reachable.
//
// The descriptor's two size fields turn out not to matter. The pointer is
// stashed at workBuffer+0x4C and never read back; every read goes through
// `DVDReadAsyncPrio(fileInfo, ...)`, which clamps against the length DVDFastOpen
// copied out of the FST. They are filled in anyway, from the same FST, so the
// borrowed entry never holds a size that contradicts its path.
//
// Because the path is only a lookup key, a missing file is detectable before
// anything is disturbed: DVDConvertPathToEntrynum returns -1. mmtProbe below
// runs first and the code stands down on a miss rather than stopping the stock
// theme and then starting nothing -- otherwise a build selecting a custom track
// would play silence on a disc that does not carry it.
//
// The stock menu theme is a Musyx FX (fx 484), not a stream, so it has to be
// silenced separately: stop its handle once, then hold the "already playing"
// guard at 0x803C6718 SET, which makes the menu music routine (0x80062A94)
// skip its start every frame.
//
// The guard stays held while muted for the Dictionary. Releasing it there would
// let fx 484 restart underneath the Dictionary's own track -- which is exactly
// the two-songs-at-once bug that scene is known for. It is only released when
// the game REL takes over, to hand the audio back untouched.

// ---- claimed RAM (see ClaimedFreeMemory.h) --------------------------------
// g_mmtStarted holds a magic value rather than a flag: claimed RAM contains
// whatever was there at power-on, and acting on a garbage "started" would stop a
// stream we never began and write a garbage pointer into the stream table.
#define MMT_MAGIC               0x4D4D5401
#define MMT_NO_FILE             0x4D4D5402           // probed once, file not on the disc
#define g_mmtStarted   VAR_ADDRESS(u32, 0x802EC2B0)  // == MMT_MAGIC while streaming
#define g_mmtSavedPath VAR_ADDRESS(u32, 0x802EC2B4)  // stock descriptor path ptr
#define g_mmtSavedSize VAR_ADDRESS(u32, 0x802EC2B8)  // stock descriptor size
#define MMT_PATH_BUF            0x802EC2C0           // 32 bytes for our path string

// ---- game ------------------------------------------------------------------
#define playStream  ((void (*)(u8))0x8006877C)
#define jukeboxStop ((void (*)(void))0x800A8F68)
#define jukeboxCmd  ((void (*)(u32))0x800A86B4)      // 4 = cancel the DVD stream
#define sndFXStop   ((void (*)(u32))0x800C832C)
#define dvdPathToEntrynum ((s32 (*)(const char*))0x8007791C)

#define STREAM_TABLE 0x800E87B4                      // 16 bytes per entry
#define STREAM_TABLE_ENTRIES 15                      // ids 0..14 (.data size 0xF0)
#define HOST_ID      14                              // descriptor borrowed for 15+
                                                     // (home_in, a short jingle)

// The jukebox's per-stream work buffers, and the head of its play queue.
#define JUKEBOX_WORK 0x8034E478                      // 80 bytes per stream id
#define JUKEBOX_HEAD 0x803CC150                      // r13 - 0x75F0

// Stream ids 15 and up have no descriptor of their own, so they have to borrow
// HOST_ID's and hand it back. 15/16 are the two unused disc tracks; 17+ are the
// custom slots.
#define MMT_NEEDS_BORROW (MMT_STREAM_ID >= 15)
#define MMT_IS_CUSTOM    (MMT_TRACK >= MMT_CUSTOM_FIRST)
#define MMT_CUSTOM_PATH_LEN 29                       // "snd/my_snd_h/custom_00_h.adp" + NUL
#define MMT_STAR_PATH_LEN   27                       // "snd/my_snd_h/star_01_h.adp"  + NUL

// Which work buffer the jukebox is using for OUR track -- a borrowed track runs
// under HOST_ID's, not its own.
#define MMT_PLAY_ID  (MMT_NEEDS_BORROW ? HOST_ID : MMT_STREAM_ID)
#define MMT_WORK_BUF (JUKEBOX_WORK + MMT_PLAY_ID * 80)

#define g_menuMusicHandle VAR_ADDRESS(u32, 0x803C6714)
#define g_menuMusicGuard  VAR_ADDRESS(u8,  0x803C6718)

#if MMT_NEEDS_BORROW

// Write the path for the selected track into claimed RAM. The game is handed a
// pointer to this buffer rather than into our own payload, which the loader has
// no guarantee of still being able to reach.
static void mmtBuildPath(void)
{
    char* p = (char*)MMT_PATH_BUF;

#if MMT_IS_CUSTOM
    // The two digits sit at fixed offsets in the template, so one literal covers
    // all ten slots. MMT_CUSTOM_NUMBER is 1..10 -> "custom_01_h".."custom_10_h".
    // The `_h` suffix is the disc's convention for every streamed track.
    memcpy(p, (void*)"snd/my_snd_h/custom_00_h.adp", MMT_CUSTOM_PATH_LEN);
    p[20] = (char)('0' + (MMT_CUSTOM_NUMBER / 10));
    p[21] = (char)('0' + (MMT_CUSTOM_NUMBER % 10));
#else
    memcpy(p, (void*)((MMT_STREAM_ID == 15) ? "snd/my_snd_h/star_01_h.adp"
                                            : "snd/my_snd_h/star_03_h.adp"),
           MMT_STAR_PATH_LEN);
#endif
}

// Look the built path up in the FST and return the file's length, or 0 if it is
// not on this disc. Pure RAM work -- the FST is resident from DVDInit onwards,
// so this is safe to call from a per-frame hook and costs nothing on a miss.
//
// FST entries are 12 bytes: { u32 isDirAndStringOff; u32 pos; u32 len }, with
// the top u8 of the first u32 set for directories. Entry 0 is the root, and
// its length field doubles as the entry count.
static u32 mmtProbe(void)
{
    u8* fst = *(u8**)0x80000038;                     // BootInfo->FSTLocation
    s32 e   = dvdPathToEntrynum((const char*)MMT_PATH_BUF);
    u8* entry;

    if (fst == 0 || e < 0 || (u32)e >= *(u32*)(fst + 8))
        return 0;

    entry = fst + (u32)e * 12;
    if (*entry != 0)                                 // a directory of that name
        return 0;

    return *(u32*)(entry + 8);
}

#endif  // MMT_NEEDS_BORROW

// The jukebox is a QUEUE, not a single slot: jukeboxPlay links a new work
// buffer in behind whatever is already going (cur->next at +4, prev at +0) and
// only starts it when the list was empty. So when the match loads and game.rel
// asks for the stadium track while our menu stream is still running, its request
// is parked BEHIND ours -- the menu music just keeps playing. Worse, the stop
// below is the jukebox's hard reset (the same call SND init makes at 0x80021A18)
// and zeroes head/cur/tail, so that parked request is thrown away with it and
// the stadium track never arrives at all.
//
// Walk the queue before resetting and pick up whatever the game asked for, so it
// can be handed straight back afterwards. Returns the stream id, or -1 if the
// only thing queued is ours. Ids come back out of the descriptor pointer, which
// is what jukeboxPlay stashed at work+0x4C.
static s32 mmtQueuedGameStream(void)
{
    u8* work = (u8*)MMT_WORK_BUF;                    // ours -- the one to ignore
    u8* node = *(u8**)JUKEBOX_HEAD;
    u32 hops;

    for (hops = 0; hops < 16; hops++)
    {
        u32 desc;

        if (node < (u8*)0x80000000 || node >= (u8*)0x81800000)
            break;

        desc = *(u32*)(node + 0x4C);
        if (node != work && desc >= STREAM_TABLE &&
            desc < STREAM_TABLE + STREAM_TABLE_ENTRIES * 16)
            return (s32)((desc - STREAM_TABLE) / 16);

        node = *(u8**)(node + 4);                    // ->next
    }
    return -1;
}

// Stop our stream and hand the borrowed descriptor back. releaseGuard is only
// set when the game REL takes over -- see the note about the Dictionary above.
static void mmtStop(u32 releaseGuard)
{
    // Only on the handover to the game. Muting for the Dictionary must not
    // re-issue anything -- that scene's track is not a stream.
    s32 queued = (releaseGuard != 0) ? mmtQueuedGameStream() : -1;

    // Cancel the DVD stream FIRST. These .adp tracks are hardware DTK:
    // DVDPrepareStreamAsync points the DRIVE at a disc region, and the reset
    // below only calls AISetStreamPlayState(0) -- it stops playback but leaves
    // the drive still pointed at our track. Whoever next asks the jukebox to
    // resume gets a re-trigger with no fresh prepare (see the state==2 path at
    // 0x800A8964) and our menu track comes back mid-match, which is exactly the
    // symptom. jukeboxCmd(4) is the game's own cancel: it zeroes the stream
    // volumes and issues DVDCancelStreamAsync with the jukebox's own command
    // block, so the drive is left holding nothing and the next playStream has
    // no choice but to prepare the track it actually wants.
    jukeboxCmd(4);

    jukeboxStop();

    if (g_mmtSavedPath >= 0x80000000 && g_mmtSavedPath < 0x81800000)
    {
        u8* e = (u8*)(STREAM_TABLE + HOST_ID * 16);
        *(u32*)(e + 0)  = g_mmtSavedPath;
        *(u32*)(e + 4)  = g_mmtSavedSize;
        *(u32*)(e + 12) = g_mmtSavedSize;
    }
    g_mmtSavedPath = 0;

    if (releaseGuard != 0)
    {
        g_menuMusicGuard  = 0;                       // let the stock music start again
        g_menuMusicHandle = 0;
    }

    // Re-issue the game's own request now the queue is clear and the borrowed
    // descriptor is back. Nothing queued means the game had not asked for
    // anything yet and will ask normally -- the common case, unchanged.
    if (queued >= 0)
        playStream((u8)queued);

    g_mmtStarted = 0;
}

void MenuMusicTrackSelect()
{
    // `rel` is GlobalData.h's own macro for 0x800E877C: 0 = boot, 4 = menu,
    // 5 = game. Do not shadow it with a local -- it is a #define.
    u16 sc = *(u16*)0x800E877E;                      // menuCtrl->screenCode

    // Play across every menu screen; mute for the Dictionary so its own track is
    // heard, and stop once the game REL loads.
    if (inningSetting.rel != 4)
    {
        if (g_mmtStarted == MMT_MAGIC)
            mmtStop(1);                              // hand the audio back
        return;
    }

    // The selected track is not on this disc. Behave exactly like MMT_DEFAULT
    // from here on -- including leaving the Dictionary alone -- rather than
    // suppressing a theme we have nothing to put in place of.
    if (g_mmtStarted == MMT_NO_FILE)
        return;

    if (sc == 7)                                     // Dictionary
    {
        if (g_mmtStarted == MMT_MAGIC)
            mmtStop(0);                              // keep fx 484 suppressed

        g_menuMusicGuard = 1;
        return;
    }

    if (g_mmtStarted != MMT_MAGIC)
    {
        u8  id = MMT_STREAM_ID;
        u32 handle;

#if MMT_NEEDS_BORROW
        {
            // Borrow a descriptor and point it at a track the game has no entry
            // for. Probe before touching anything: on a miss we latch NO_FILE
            // and never come back, leaving the stock theme untouched.
            u8* e = (u8*)(STREAM_TABLE + HOST_ID * 16);
            u32 len;

            mmtBuildPath();

            len = mmtProbe();
            if (len == 0)
            {
                g_mmtStarted = MMT_NO_FILE;
                return;
            }

            g_mmtSavedPath = *(u32*)(e + 0);
            g_mmtSavedSize = *(u32*)(e + 4);

            *(u32*)(e + 0)  = MMT_PATH_BUF;
            *(u32*)(e + 4)  = len;
            *(u32*)(e + 12) = len;

            id = HOST_ID;
        }
#endif

        // Silence the Musyx menu theme (it is an FX, not a stream). Only once
        // the track above is known to be openable -- see the probe.
        handle = g_menuMusicHandle;
        if (handle != 0 && handle != 0xFFFFFFFF)
            sndFXStop(handle);
        g_menuMusicHandle = 0;

        // Clear the queue before asking for ours. The game stops a stream by
        // dropping the jukebox to state 0 and leaves it LINKED, so coming back
        // from a match the stadium track is still head -- measured, on the way
        // out of a game: `w0:mario_01_h.adp -> w13:cha_s_roll_h.adp`. Appending
        // behind it means the stale track resumes and ours never starts.
        jukeboxStop();

        playStream(id);
        g_mmtStarted = MMT_MAGIC;
    }

    // Hold the guard set so 0x80062A94 never restarts the stock menu theme
    // underneath our stream.
    g_menuMusicGuard = 1;
}

// ===========================================================================
#endif
// ===========================================================================
