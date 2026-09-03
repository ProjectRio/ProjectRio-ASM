/*###########################################################
# Custom Music
###########################################################*/
// Author: LittleCoaks
//
// *Choose the music for the main menu and for every streamed track the game
// *has -- all seven stadiums, plus replay, results, victory, the challenge
// *tracks and the jingles.
// *Any of the game's own soundtracks, the two tracks that ship on the disc but
// *the stock game can never play, or your own files dropped in as
// *snd/my_snd_h/custom_01_h.adp .. custom_10_h.adp.
// *A track whose file is not on the disc is never offered and never played.
//
// PART OF THE PACK, NOT A STANDALONE CODE -- which is why it lives here rather
// than in "Gecko Codes/". It has eight settings and no on/off, so it is only
// useful with a UI in front of it, and that UI is RioModPack's Options menu.
// Shipping it as a lone ini code would give you eight words of claimed RAM and
// no way to reach them. (build_all.py scans "Gecko Codes/" only, so sitting
// here also keeps it out of the standalone sweep, where it would build but be
// pointless.)
//
// Configured at RUNTIME through RioModPack/MusicConfig.h -- eight slots, one
// per stadium plus the menu. The slots are plain claimed RAM, so an ini code
// can still set them if you want the mod without the menu.
//
// THE DICTIONARY THEME is the one track that is neither. It is a Musyx FX
// layer, not a stream, so there is no descriptor to retarget and no file to
// point at -- swapping it in means loading another scene's audio group and
// repointing fx 484, past a moving FX table and a fade path that bricks the
// menu if you touch it. All of that already exists, carefully, in
// "Gecko Codes/Menu/Dictionary Replaces Menu Music.c", so this mod does not
// reimplement a word of it: the pack includes that file and hands it a
// CGECKO_ACTIVE expression reading THIS config, and it switches itself on and off
// against the menu slot. Being an FX rather than a stream is also why only the
// MENU slot can select it -- see MusicTrackAvailable().
//
// (This was "Menu Music Track Select", which picked ONE track for the menu at
// build time through an MMT_TRACK #define, with the Dictionary swap as a third
// compile-time branch.)
//
// Deliberately declared with NO `.state`, so it runs in every state. A
// .state = MSSB_MENU code stops being executed the moment the game REL loads,
// which would leave the menu stream running into the match with nothing left to
// shut it off.
//
// ===========================================================================
// TWO COMPLETELY DIFFERENT MECHANISMS
// ===========================================================================
// THE STADIUMS retarget the game's own stream descriptors and then get out of
// the way. `playStream(id)` @0x8006877C is table driven: the descriptor at
// 0x800E87B4[id] is 16 bytes {char* path, u32 size, u32, u32 size}, and the
// path is resolved through the disc FST at RUNTIME (0x800A7544 does
// DVDConvertPathToEntrynum, then 0x800A750C does DVDFastOpen). Nothing about a
// filename is baked into the game, so pointing Mario Stadium's descriptor at
// another path is enough to change what the game plays when it asks for its own
// stadium music. We never intercept the call -- the game plays the file its
// descriptor names. That is also what makes the two unused tracks reachable
// (they ARE in the FST, they simply have no descriptor) and what makes a file
// the user added reachable.
//
// The descriptor's two size fields turn out not to matter: the pointer is
// stashed at workBuffer+0x4C and never read back, and every read goes through
// DVDReadAsyncPrio, which clamps against the length DVDFastOpen copied out of
// the FST. They are filled in anyway, from that same FST, so a retargeted entry
// never holds a size that contradicts its path.
//
// THE MENU cannot use any of that: the main-menu theme is a Musyx FX (fx 484),
// not a stream. So the menu slot BORROWS one descriptor (home_in, a short
// jingle nothing needs during the menu), points it at the chosen track, starts
// a stream by hand, and silences fx 484 separately. It always borrows -- even
// for one of the game's own tracks -- so that the menu is unaffected by
// whatever the stadium slots have done to their descriptors. Setting the menu
// to Mario Stadium and Mario Stadium to a custom track has to give two
// different songs.
//
// A missing file is detectable before anything is disturbed: the FST lookup
// returns -1. Every path is probed BEFORE the stock music is stopped, so a slot
// pointed at a file this disc does not carry leaves the game exactly as it was
// rather than stopping the stock theme and then starting nothing.
#include "Include/game/UnknownHomes_Game.h"

#include "Include/static/UnknownHomes_Static.h"
#include "Include/musyx/musyx.h"
#include "RioModPack/MusicConfig.h"

// ---- claimed RAM (see ClaimedFreeMemory.h) --------------------------------
// g_mmtStarted holds a magic value rather than a flag: claimed RAM contains
// whatever was there at power-on, and acting on a garbage "started" would stop
// a stream we never began and write a garbage pointer into the stream table.
#define MMT_MAGIC               0x4D4D5401
#define g_mmtStarted   VAR_ADDRESS(u32, 0x802EC2B0)  // == MMT_MAGIC while streaming
#define g_mmtTrack     VAR_ADDRESS(u32, 0x802EC2BC)  // the track currently streaming
#define MMT_PATH_BUF            0x802EC2C0           // 32 bytes for the menu's path

#define g_musicMagic   VAR_ADDRESS(u32, MUSICCFG_MAGIC_ADDR)

// ---- game ------------------------------------------------------------------
#define playStream  ((void (*)(u8))0x8006877C)
#define jukeboxStop ((void (*)(void))0x800A8F68)
#define jukeboxCmd  ((void (*)(u32))0x800A86B4)      // 4 = cancel the DVD stream
#define sndFXStop   ((void (*)(u32))0x800C832C)

// The menu borrows one stream descriptor to play through. home_in is the least
// disruptive choice -- a short jingle nothing needs while the menu is up -- but
// it is ALSO slot 15 now that every stream is configurable, so the two have to
// cooperate: see musicApplyStreams() and mmtStop().
#define HOST_ID         14                           // home_in
#define MUSIC_HOST_SLOT 15                           // the slot that drives it
#define JUKEBOX_WORK 0x8034E478                      // 80 bytes per stream id
#define JUKEBOX_HEAD 0x803CC150                      // r13 - 0x75F0
#define MMT_WORK_BUF (JUKEBOX_WORK + HOST_ID * 80)

#define g_menuMusicHandle VAR_ADDRESS(u32, 0x803C6714)
#define g_menuMusicGuard  VAR_ADDRESS(u8,  0x803C6718)

#define STREAM_ENTRY(id) ((u8*)(MUSIC_STREAM_TABLE + (id) * 16))
#define SAVED_ENTRY(slot) ((u32*)(MUSIC_SAVED_BASE + ((slot) - 1) * 8))

// ---------------------------------------------------------------------------
// One-shot: capture each stadium's stock descriptor before anything retargets
// it, and start every slot on Default. Both have to happen before the first
// write, and both have to happen exactly once per console session -- re-saving
// after a retarget would record OUR path as the stock one and the slot could
// never be put back.
// ---------------------------------------------------------------------------
static void musicInitOnce(void)
{
    int slot;

    if (g_musicMagic == MUSICCFG_MAGIC)
        return;
    g_musicMagic = MUSICCFG_MAGIC;

    for (slot = 1; slot < MUSIC_SLOT_COUNT; slot++)
    {
        u8*  e = STREAM_ENTRY(s_musicSlotStream[slot]);
        u32* s = SAVED_ENTRY(slot);

        s[0] = *(u32*)(e + 0);
        s[1] = *(u32*)(e + 4);
    }
    MusicConfig_Reset();
}

// ---------------------------------------------------------------------------
// Resolve a track to the {path, size} a descriptor should hold. Returns 0 and
// leaves *path / *size alone when the track is not playable on this disc, which
// every caller treats as "leave the slot on its stock music".
//
// `scratch` is where a star/custom path string is built. It must be RAM the
// game can still reach later -- the descriptor keeps the pointer -- so callers
// pass claimed RAM, never a local.
// ---------------------------------------------------------------------------
// What stream id `id` ORIGINALLY named. The seven stadium ids get retargeted,
// so their live descriptor is not a reliable source once anything is applied:
// "Bowser Castle plays Mario Stadium's music" has to mean the real mario_01,
// not whatever Mario Stadium's slot has since been pointed at. Every other id
// is never written, so its live entry is its original.
static void musicStockDesc(u32 id, u32* path, u32* size)
{
    int slot;

    for (slot = 1; slot < MUSIC_SLOT_COUNT; slot++)
    {
        if (s_musicSlotStream[slot] == id)
        {
            u32* s = SAVED_ENTRY(slot);
            *path = s[0];
            *size = s[1];
            return;
        }
    }
    {
        u8* e = STREAM_ENTRY(id);
        *path = *(u32*)(e + 0);
        *size = *(u32*)(e + 4);
    }
}

static u32 musicResolve(u32 track, char* scratch, u32* path, u32* size)
{
    if (MUSIC_IS_STOCK(track))
    {
        musicStockDesc(MUSIC_TRACK_STREAM(track), path, size);
        return 1;
    }
    if (MUSIC_IS_STAR(track) || MUSIC_IS_CUSTOM(track))
    {
        u32 len;

        MusicBuildPath(track, scratch);
        len = MusicProbePath(scratch);
        if (len == 0)
            return 0;                                // not on this disc
        *path = (u32)scratch;
        *size = len;
        return 1;
    }
    return 0;                                        // Default, or out of range
}

// ---------------------------------------------------------------------------
// STADIUMS. Idempotent: work out what each descriptor should say and write it
// only when it differs, so this can run every frame and self-corrects after the
// user changes a slot. Never runs during a match -- see the caller.
// ---------------------------------------------------------------------------
static void musicApplySlot(int slot)
{
    u8*  e     = STREAM_ENTRY(s_musicSlotStream[slot]);
    u32* saved = SAVED_ENTRY(slot);
    char* buf  = (char*)(MUSIC_PATHBUF_BASE + (slot - 1) * MUSIC_PATHBUF_SIZE);
    u32  track = MusicSlotTrack(slot);
    u32  path  = saved[0];                           // Default, and the fallback
    u32  size  = saved[1];

    // A track this disc does not carry leaves path/size on the stock pair
    // above, so the slot quietly stays on its own music.
    if (track != MUSIC_DEFAULT)
        musicResolve(track, buf, &path, &size);

    if (*(u32*)(e + 0) != path || *(u32*)(e + 4) != size)
    {
        *(u32*)(e + 0)  = path;
        *(u32*)(e + 4)  = size;
        *(u32*)(e + 12) = size;
    }
}

// Every stream slot. Idempotent: each one is recomputed from the config and the
// captured original and written only when it differs, so this can run every
// frame and self-corrects the moment the user changes a slot.
//
// The host slot is skipped while the menu stream is running: the menu has
// BORROWED that descriptor and pointed it at its own track, and rewriting it
// underneath a playing stream would leave the jukebox holding a descriptor that
// no longer describes what it opened. mmtStop() applies it as it hands the
// descriptor back, so the slot is never stale for longer than the menu stream
// lasts -- and a match, which is where most of these tracks are heard, only
// starts after that handover.
static void musicApplyStreams(void)
{
    int slot;

    for (slot = 1; slot < MUSIC_SLOT_COUNT; slot++)
    {
        if (slot == MUSIC_HOST_SLOT && g_mmtStarted == MMT_MAGIC)
            continue;
        musicApplySlot(slot);
    }
}

// ---------------------------------------------------------------------------
// The jukebox is a QUEUE, not a single slot: jukeboxPlay links a new work
// buffer in behind whatever is already going (cur->next at +4, prev at +0) and
// only starts it when the list was empty. So when the match loads and game.rel
// asks for the stadium track while our menu stream is still running, its
// request is parked BEHIND ours -- the menu music just keeps playing. Worse,
// the stop below is the jukebox's hard reset (the same call SND init makes at
// 0x80021A18) and zeroes head/cur/tail, so that parked request is thrown away
// with it and the stadium track never arrives at all.
//
// Walk the queue before resetting and pick up whatever the game asked for, so
// it can be handed straight back afterwards. Returns the stream id, or -1 if
// the only thing queued is ours. Ids come back out of the descriptor pointer,
// which is what jukeboxPlay stashed at work+0x4C.
// ---------------------------------------------------------------------------
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
        if (node != work && desc >= MUSIC_STREAM_TABLE &&
            desc < MUSIC_STREAM_TABLE + MUSIC_STREAM_COUNT * 16)
            return (s32)((desc - MUSIC_STREAM_TABLE) / 16);

        node = *(u8**)(node + 4);                    // ->next
    }
    return -1;
}

// Stop the menu stream and hand the borrowed descriptor back. releaseGuard is
// only set when the game REL takes over: muting for the Dictionary must keep
// fx 484 suppressed, or it restarts underneath that scene's own track -- which
// is exactly the two-songs-at-once bug that scene is known for.
static void mmtStop(u32 releaseGuard)
{
    s32 queued = (releaseGuard != 0) ? mmtQueuedGameStream() : -1;

    // Cancel the DVD stream FIRST. These .adp tracks are hardware DTK:
    // DVDPrepareStreamAsync points the DRIVE at a disc region, and the reset
    // below only calls AISetStreamPlayState(0) -- it stops playback but leaves
    // the drive still pointed at our track. Whoever next asks the jukebox to
    // resume gets a re-trigger with no fresh prepare (see the state==2 path at
    // 0x800A8964) and the menu track comes back mid-match, which is exactly the
    // symptom. jukeboxCmd(4) is the game's own cancel: it zeroes the stream
    // volumes and issues DVDCancelStreamAsync with the jukebox's own command
    // block, so the drive is left holding nothing and the next playStream has
    // no choice but to prepare the track it actually wants.
    jukeboxCmd(4);
    jukeboxStop();

    // Hand the borrowed descriptor back to whatever its OWN slot is configured
    // to, not to the stock track: Home Run Jingle is a slot like any other now,
    // and restoring a saved copy would quietly undo the user's setting for it
    // every time the menu stream stopped. musicApplySlot recomputes from the
    // config and the original captured at boot, so it is right either way.
    g_mmtStarted = 0;                                // cleared HERE, before the
    musicApplySlot(MUSIC_HOST_SLOT);                 // apply, so its skip lifts

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
}

// Start the menu stream on `track`. Probes first: on a miss nothing is touched
// and the stock theme keeps playing.
static void mmtStart(u32 track)
{
    u8*  e = STREAM_ENTRY(HOST_ID);
    u32  path, size, handle;

    if (!musicResolve(track, (char*)MMT_PATH_BUF, &path, &size))
        return;

    *(u32*)(e + 0)  = path;
    *(u32*)(e + 4)  = size;
    *(u32*)(e + 12) = size;

    // Silence the Musyx menu theme (it is an FX, not a stream). Only once the
    // track above is known to be openable -- see the probe.
    handle = g_menuMusicHandle;
    if (handle != 0 && handle != 0xFFFFFFFF)
        sndFXStop(handle);
    g_menuMusicHandle = 0;

    // Clear the queue before asking for ours. The game stops a stream by
    // dropping the jukebox to state 0 and leaves it LINKED, so coming back from
    // a match the stadium track is still head -- measured, on the way out of a
    // game: `w0:mario_01_h.adp -> w13:cha_s_roll_h.adp`. Appending behind it
    // means the stale track resumes and ours never starts.
    jukeboxStop();

    playStream(HOST_ID);
    g_mmtStarted = MMT_MAGIC;
    g_mmtTrack   = track;
}

CGECKO(CustomMusic,
       // No '_' here on purpose -- the font has no underscore glyph and would
       // draw the filenames as "custom?01?h.adp". They are spelled out in the
       // file header and in the ini description, which have no such limit.
       .notes = "Set the music for the menu and every streamed track. Custom "
                ".adp files on the disc are offered too.");
void CustomMusic()
{
    // `rel` is GlobalData.h's own macro for 0x800E877C: 0 = boot, 4 = menu,
    // 5 = game. Do not shadow it with a local -- it is a #define.
    u16 sc = *(u16*)0x800E877E;                      // menuCtrl->screenCode
    u32 want;

    musicInitOnce();

    // Outside the menus, hand the audio back and leave the stadium descriptors
    // alone. Retargeting one mid-match would be read by whatever the game
    // starts next, and the match has already asked for its music by then.
    if (inningSetting.rel != 4)
    {
        if (g_mmtStarted == MMT_MAGIC)
            mmtStop(1);
        return;
    }

    // Stream slots are applied from the menu, which is the only place they can
    // be changed and is always before whatever reads them.
    musicApplyStreams();

    want = MusicSlotTrack(MUSIC_SLOT_MENU);

    // The Dictionary theme is the one track that is not a stream: it is a Musyx
    // FX layer, swapped in by "Dictionary Replaces Menu Music" (included in the
    // pack and driven by this same slot word). That code works THROUGH the
    // game's own menu-music routine, so we have to hand the routine back
    // completely -- guard included, or it can never restart the track. Checked
    // before the screenCode 7 case below so the guard is never held in this mode.
    if (want == MUSIC_DICTIONARY)
    {
        if (g_mmtStarted == MMT_MAGIC)
            mmtStop(1);
        return;
    }

    if (sc == 7)                                     // Dictionary scene
    {
        if (g_mmtStarted == MMT_MAGIC)
            mmtStop(0);                              // keep fx 484 suppressed
        g_menuMusicGuard = 1;
        return;
    }

    if (want == MUSIC_DEFAULT)
    {
        // Back to the stock theme: stop ours and release the guard so
        // 0x80062A94 starts fx 484 again on its next pass.
        if (g_mmtStarted == MMT_MAGIC)
            mmtStop(1);
        return;
    }

    // Changing the menu track while it plays: stop the old one, then fall
    // through and start the new one on the next frame's pass.
    if (g_mmtStarted == MMT_MAGIC && g_mmtTrack != want)
    {
        mmtStop(0);
        return;
    }

    if (g_mmtStarted != MMT_MAGIC)
        mmtStart(want);

    // Hold the guard set so 0x80062A94 never restarts the stock menu theme
    // underneath our stream. Only while we actually got a stream going -- on a
    // missing file mmtStart() stands down and the stock theme must keep playing.
    if (g_mmtStarted == MMT_MAGIC)
        g_menuMusicGuard = 1;
}
