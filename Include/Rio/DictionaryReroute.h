/*###########################################################
# DictionaryReroute.h -- shared reroute + music fix for the Dictionary scene
###########################################################*/
// Author: LittleCoaks
//
// Call DictionaryReroute_Tick() once per frame from a per-frame code -- a
// CGECKO() with no .address and .state = MSSB_MENU. It:
//
//  1) REROUTE -- patches the main-menu dispatch so the "Records" button opens
//     the unused Dictionary scene (screenCode 7) instead of Records (8). The
//     dispatch at 0x80641848 is `li r3,8; bl changeScreenVariables`; we write
//     `li r3,7` (0x38600007) over it every frame while the menu REL is resident.
//
//  2) MUSIC FIX (reverse-engineered live, 2026-07-24) -- the menu BGM is a
//     single looping FX voice (macro 484, played by the routine at 0x80062A94
//     as `sndFXStartEx(484)`, gated by an "already playing" guard u8 at
//     0x803C6718). The stock Dictionary scene starts its own track without
//     stopping the menu voice, so two songs layer. On ENTERING Dictionary we
//     kill the menu voice (synthKillAllVoices) so only the Dictionary track
//     plays; on LEAVING we clear the guard so the menu-music routine replays
//     484 and the menu music resumes. Both are edge-triggered off screenCode.

#ifndef DICTIONARY_REROUTE_H
#define DICTIONARY_REROUTE_H
#include "Include/game/UnknownHomes_Game.h"
#include "Include/menus/yd_step.h"
#include "Include/musyx/musyx.h"

// Claimed RAM: last frame's screenCode (edge detection) + a "resuming" latch.
#define g_dictPrevScreen VAR_ADDRESS(u16, 0x802EC280)
#define g_dictResuming   VAR_ADDRESS(u32, 0x802EC284)

// The menu-music routine's handle slot: sndFXStartEx return, or 0xFFFFFFFF on a
// FAILED start (e.g. its sound bank isn't loaded yet).
#define g_menuMusicHandle VAR_ADDRESS(u32, 0x803C6714)
#define g_menuMusicGuard  VAR_ADDRESS(unsigned char, 0x803C6718)

static inline void DictionaryReroute_Tick(void)
{
    // 1) Records button -> Dictionary (screenCode 7).
    *(volatile u32*)0x80641848 = 0x38600007;            // li r3, 7

    u16 sc   = *(u16*)(*(u32*)0x803CBBCC + 2);           // menuCtrl->screenCode
    u16 prev = g_dictPrevScreen;

    if (sc == 7 && prev != 7)
    {
        // ENTER Dictionary: stop the still-playing menu BGM voice so it doesn't
        // layer with the Dictionary track. Also mark the routine's handle
        // invalid, so the resume logic below can't be fooled by the stale
        // (pre-kill) handle into thinking the music is still playing.
        ((void(*)(unsigned char))0x800D15FC)(0);        // synthKillAllVoices(0)
        g_menuMusicHandle = 0xFFFFFFFF;
    }
    else if (sc != 7 && prev == 7)
    {
        g_dictResuming = 0x0D1C7;                        // LEAVE: begin resuming
    }

    // RESUME: back on the main menu, clear the menu-music "already playing" guard
    // so the routine (0x80062A94) replays sfxId 484. Right after leaving, the
    // menu's sound bank is still reloading, so sndFXStartEx can fail (handle stays
    // 0xFFFFFFFF); keep retrying each frame until a real handle appears, then stop
    // (clearing further would replay on top and double the music).
    if (sc == 5 && g_dictResuming == 0x0D1C7)
    {
        u32 handle = g_menuMusicHandle;
        if (handle == 0 || handle == 0xFFFFFFFF)
            g_menuMusicGuard = 0;                        // retry the start
        else
            g_dictResuming = 0;                          // playing -> done
    }

    g_dictPrevScreen = sc;
}

#endif
