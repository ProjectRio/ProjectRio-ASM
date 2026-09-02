/*###########################################################
# MusicConfig.h -- the soundtrack slots, their tracks, and how to find a file
###########################################################*/
// Author: LittleCoaks
//
// Shared by the mod that PLAYS the music ("RioModPack/Custom Music.c") and the
// UI that CONFIGURES it ("RioModPack/Options Menu.c"), so the two can never
// disagree about what track 19 is. Both are pack files, which is why this sits
// beside them rather than in Include/Rio/ with the general-purpose headers.
//
// Everything here is `static`: the pack is one translation unit, so there is
// exactly one copy, and a file that includes this on its own still builds.
//
// EIGHT SLOTS. One for the menu, one per stadium. A slot holds a TRACK id (see
// below); 0 = MUSIC_DEFAULT means "leave the game's own music alone", which is
// what every slot reads as after ModOptions_Reset().
//
// HOW A SLOT IS APPLIED (the mod's half, summarised here because it explains
// the track numbering):
//   * Stadium slots retarget the game's own stream descriptor. Each stream id
//     has a 16-byte entry at STREAM_TABLE holding {char* path, u32 size, ...},
//     and the path is resolved through the disc FST at RUNTIME -- so pointing
//     Mario Stadium's entry at another track's path is enough to change what
//     the game plays when it asks for its own stadium music. Nothing is
//     intercepted; the game plays the file the descriptor names.
//   * The menu slot cannot use that trick, because the menu music is a Musyx FX
//     rather than a stream. It borrows one descriptor and starts a stream by
//     hand -- see the mod.
//
// A MISSING FILE IS NEVER SELECTABLE. MusicTrackStep() skips any track whose
// file is not on this disc, so a custom slot with nothing behind it cannot be
// chosen in the first place, and the mod probes again before it commits to
// anything. Both halves check because the config is plain RAM: a value can also
// arrive from an ini code or a stale byte, not just from the menu.

#ifndef MUSICCONFIG_H
#define MUSICCONFIG_H

#include "CGecko/Common.h"
#include "Include/types.h"

// ---- tracks ---------------------------------------------------------------
// Track ids are a flat list so the UI can just step through them.
//   0        leave the game's own music alone
//   1..15    the game's own streams, track id N -> stream id N-1
//   16,17    star_01 / star_03: on the disc, unreachable by the stock game
//   18..27   snd/my_snd_h/custom_01_h.adp .. custom_10_h.adp
#define MUSIC_DEFAULT        0
#define MUSIC_STOCK_FIRST    1
#define MUSIC_STOCK_COUNT   15
#define MUSIC_STAR_FIRST    16
#define MUSIC_STAR_COUNT     2
#define MUSIC_CUSTOM_FIRST  18
#define MUSIC_CUSTOM_COUNT  10
// The Dictionary scene's theme. NOT a stream -- it is a Musyx FX layer, so it
// is the one track that cannot be reached by retargeting a descriptor, and the
// one track a stadium slot cannot use. See MusicTrackAvailable(). Appended
// last on purpose: a slot word holds a track id, so inserting it anywhere else
// would silently re-point every configured slot.
#define MUSIC_DICTIONARY    28
#define MUSIC_TRACK_COUNT   29

#define MUSIC_TRACK_STREAM(t) ((t) - MUSIC_STOCK_FIRST)   // valid for stock ids
#define MUSIC_IS_STOCK(t)  ((t) >= MUSIC_STOCK_FIRST  && (t) < MUSIC_STOCK_FIRST + MUSIC_STOCK_COUNT)
#define MUSIC_IS_STAR(t)   ((t) >= MUSIC_STAR_FIRST   && (t) < MUSIC_STAR_FIRST + MUSIC_STAR_COUNT)
#define MUSIC_IS_CUSTOM(t) ((t) >= MUSIC_CUSTOM_FIRST && (t) < MUSIC_CUSTOM_FIRST + MUSIC_CUSTOM_COUNT)

// ---- slots ----------------------------------------------------------------
// Slot 0 is the menu. Slots 1..15 are EVERY streamed track the game has, one
// per stream id -- not just the stadiums. They all work the same way (retarget
// a descriptor), so there is no reason to offer only some of them: the replay,
// results, victory and challenge tracks are swappable for exactly the same
// cost as Mario Stadium.
#define MUSIC_SLOT_MENU    0
#define MUSIC_SLOT_COUNT   16

// ---- claimed RAM (see ClaimedFreeMemory.h) --------------------------------
#define MUSICCFG_BASE      0x802EB540    // 16 words, one per slot
#define MUSIC_PATHBUF_BASE 0x802EB610    // 15 x 32 bytes, one per stream slot
#define MUSIC_PATHBUF_SIZE 32
#define MUSIC_SAVED_BASE   0x802EB590    // 15 x 8 bytes: each stream's stock
                                         // {path,size}, captured once
#define MUSICCFG_MAGIC_ADDR 0x802EB580   // one-shot init sentinel
#define MUSICCFG_MAGIC     0x4D555332    // 'MUS2' -- bumped with the layout, so
                                         // a stale sentinel from the 8-slot
                                         // build re-initialises instead of
                                         // being trusted

#define MusicSlot(i) (*(volatile u32*)(MUSICCFG_BASE + (i) * 4))

// The stream id each slot drives. Slot 0 (menu) has none -- the menu is not a
// stream -- so it is parked on 0xFF rather than a real id. Every other slot is
// simply its own index minus one, but the table stays explicit: it is what
// pairs a label with an id, and a mistake here would silently retarget the
// wrong track.
static const u8 s_musicSlotStream[MUSIC_SLOT_COUNT] =
{
    0xFF,   /* menu             */
    0,      /* Mario Stadium    */
    1,      /* Bowser Castle    */
    2,      /* Wario Palace     */
    3,      /* Yoshi Park       */
    4,      /* Peach Garden     */
    5,      /* DK Jungle        */
    6,      /* Replay           */
    7,      /* Results          */
    8,      /* Victory          */
    9,      /* Toy Field        */
    10,     /* Challenge Map    */
    11,     /* Demo             */
    12,     /* Ending Jingle    */
    13,     /* Staff Roll       */
    14,     /* Home Run Jingle  */
};

static const char* const s_musicSlotLabel[MUSIC_SLOT_COUNT] =
{
    "Menu",
    "Mario Stadium", "Bowser Castle", "Wario Palace", "Yoshi Park",
    "Peach Garden",  "DK Jungle",     "Replay",       "Results",
    "Victory",       "Toy Field",     "Challenge Map", "Demo",
    "Ending Jingle", "Staff Roll",    "Home Run Jing",
};

// Kept to 15 glyphs -- the Options menu draws these in a fixed column.
static const char* const s_musicTrackLabel[MUSIC_TRACK_COUNT] =
{
    "Default",
    "Mario Stadium", "Bowser Castle", "Wario Palace", "Yoshi Park",
    "Peach Garden",  "DK Jungle",     "Replay",       "Results",
    "Victory",       "Toy Field",     "Challenge Map", "Demo",
    "Ending Jingle", "Staff Roll",    "Home Run Jing",
    "Star 01", "Star 03",
    "Custom 01", "Custom 02", "Custom 03", "Custom 04", "Custom 05",
    "Custom 06", "Custom 07", "Custom 08", "Custom 09", "Custom 10",
    "Dictionary",
};

// ---- disc lookup ----------------------------------------------------------
#define MusicPathToEntrynum ((s32 (*)(const char*))0x8007791C)  // DVDConvertPathToEntrynum
#define MUSIC_STREAM_TABLE  0x800E87B4                          // 16 bytes per stream id
#define MUSIC_STREAM_COUNT  15

/* Build the disc path for a star/custom track into `out` (>= 32 bytes). The
 * two digits sit at fixed offsets in the template, so one literal covers all
 * ten custom slots. The `_h` suffix is the disc's convention for every
 * streamed track. Stock tracks have no path of their own here -- theirs is
 * read out of the stream table. */
static void MusicBuildPath(u32 track, char* out)
{
    const char* src;
    int i;

    if (MUSIC_IS_CUSTOM(track))
        src = "snd/my_snd_h/custom_00_h.adp";
    else if (track == MUSIC_STAR_FIRST)
        src = "snd/my_snd_h/star_01_h.adp";
    else
        src = "snd/my_snd_h/star_03_h.adp";

    for (i = 0; i < MUSIC_PATHBUF_SIZE - 1 && src[i] != 0; i++)
        out[i] = src[i];
    out[i] = 0;

    if (MUSIC_IS_CUSTOM(track))
    {
        u32 n = track - MUSIC_CUSTOM_FIRST + 1;      /* 1..10 */
        out[20] = (char)('0' + (n / 10));
        out[21] = (char)('0' + (n % 10));
    }
}

/* The file's length, or 0 when it is not on this disc.
 *
 * Pure RAM work: the FST is resident from DVDInit onwards, so this is safe from
 * a per-frame hook and costs nothing on a miss. FST entries are 12 bytes
 * { u32 isDirAndStringOff; u32 pos; u32 len }, the top byte of the first word
 * marking a directory; entry 0 is the root and its length is the entry count. */
static u32 MusicProbePath(const char* path)
{
    u8* fst = *(u8**)0x80000038;                     /* BootInfo->FSTLocation */
    s32 e;
    u8* entry;

    if (fst == 0)
        return 0;
    e = MusicPathToEntrynum(path);
    if (e < 0 || (u32)e >= *(u32*)(fst + 8))
        return 0;

    entry = fst + (u32)e * 12;
    if (*entry != 0)                                 /* a directory of that name */
        return 0;
    return *(u32*)(entry + 8);
}

/* Can this slot play this track on this disc?
 *
 * Stock streams always can -- they are what the game itself plays. Star and
 * custom tracks are files that may or may not have been added, so they get
 * looked up. The Dictionary theme is a Musyx FX rather than a stream, driven by
 * the game's own menu-music routine, so ONLY the menu slot can have it: there
 * is no descriptor to retarget for a stadium, and a match does not run that
 * routine at all. */
static u32 MusicTrackAvailable(u32 slot, u32 track, char* scratch)
{
    if (track == MUSIC_DEFAULT || MUSIC_IS_STOCK(track))
        return 1;
    if (track == MUSIC_DICTIONARY)
        return slot == MUSIC_SLOT_MENU;
    if (!MUSIC_IS_STAR(track) && !MUSIC_IS_CUSTOM(track))
        return 0;                                    /* out of range */
    MusicBuildPath(track, scratch);
    return MusicProbePath(scratch) != 0;
}

/* Step to the next selectable track, skipping any whose file is absent, so a
 * custom slot with nothing behind it can never be chosen. Wraps both ways.
 * Falls back to MUSIC_DEFAULT if a full lap finds nothing, which cannot happen
 * (Default and the stock streams are always available) but keeps the loop
 * bounded rather than trusting that. */
static u32 MusicTrackStep(u32 slot, u32 track, int dir, char* scratch)
{
    u32 t = (track < MUSIC_TRACK_COUNT) ? track : MUSIC_DEFAULT;
    int i;

    for (i = 0; i < MUSIC_TRACK_COUNT; i++)
    {
        if (dir > 0)
            t = (t + 1 < MUSIC_TRACK_COUNT) ? t + 1 : 0;
        else
            t = (t > 0) ? t - 1 : MUSIC_TRACK_COUNT - 1;

        if (MusicTrackAvailable(slot, t, scratch))
            return t;
    }
    return MUSIC_DEFAULT;
}

/* A slot's configured track, clamped. The config is plain RAM, so it can hold
 * a stale word from before this code existed or a value an ini code wrote;
 * anything out of range reads as Default rather than indexing off the end of
 * the label table or asking the jukebox for a stream that does not exist. */
static u32 MusicSlotTrack(u32 slot)
{
    u32 t = MusicSlot(slot);
    return (t < MUSIC_TRACK_COUNT) ? t : MUSIC_DEFAULT;
}

/* Zero every slot. Called once per session alongside ModOptions_Reset(). */
static void MusicConfig_Reset(void)
{
    volatile u32* cfg = (volatile u32*)MUSICCFG_BASE;
    int i;

    for (i = 0; i < MUSIC_SLOT_COUNT; i++)
        cfg[i] = MUSIC_DEFAULT;
}

#endif /* MUSICCONFIG_H */
