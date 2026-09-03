/*###########################################################
# Rollback Seed Test  (mid-play completeness probe)
###########################################################*/
// Author: LittleCoaks (harness drafted with Claude)
//
// Diagnostic, not a shipping feature -- leave it DISABLED in the ini except when
// running the test.
//
// Settles the open question for MSSB rollback: is the ~18 KB logical seed (the
// replay-snapshot structs + RNG + play counters) COMPLETE enough to restore at an
// ARBITRARY mid-play frame? Hook = C2 at simulate1FrameOfTheGame (0x80699D34),
// once per frame, before the logic step.
//
// v3: no ScreenText (the earlier version crashed BEFORE the test even ran --
// savestate showed the scratch untouched and the fault by the text engine, so the
// C2-driven ScreenText was the culprit). Results now go to claimed RAM you watch
// in Dolphin's memory viewer (or savestate for offline read):
//   0x802EBFC0  HEARTBEAT   increments every frame the hook runs (proves it runs)
//   0x802EBFC4  MODE        0 idle, 1 armed, 2 waitReal, 3 waitReplay
//   0x802EBFC8  RESULT      0 none, 1 COMPLETE (seed sufficient), 2 INCOMPLETE
//   0x802EBFCC  HASH_A      checksum of the real  next frame
//   0x802EBFD0  HASH_B      checksum of the replay next frame
// (add 0x802EBFC0..0x802EBFD4 to ClaimedFreeMemory.h)
//
// METHOD: 3-frame state machine, one step per real frame, no re-entrancy.
//   armed    : save 21-region seed to SCRATCH.            sim runs -> N+1_real
//   waitReal : HASH_A = checksum(band); restore seed.     sim runs -> N+1_replay
//   waitRepl : HASH_B = checksum(band); RESULT = (A==B).
// Trigger: hold Z on P1 during a live ball (ball in flight after contact); DO NOT
// touch the stick for ~3 frames (both sim passes must see identical inputs).

#include "Include/game/UnknownHomes_Game.h"

#include "Include/static/UnknownHomes_Static.h"
#include "Include/text/text_channel.h"
#define SCRATCH   0x815B4000   // verified-free high MEM1 (2 MB zero run); NOT a game buffer
#define N_SEED    21
#define HASH_LO   0x8088A7E4
#define HASH_HI   0x80893AA0

#define RT_HEART  VAR_ADDRESS(u32, 0x802EBFC0)
#define RT_MODE   VAR_ADDRESS(u32, 0x802EBFC4)
#define RT_RESULT VAR_ADDRESS(u32, 0x802EBFC8)
#define RT_HASHA  VAR_ADDRESS(u32, 0x802EBFCC)
#define RT_HASHB  VAR_ADDRESS(u32, 0x802EBFD0)

// The logical seed: 19 replay-snapshot structs + RNG + play counters.
static struct { u32 addr; u32 size; } SEED[N_SEED] = {
    {0x8089298C, 0x158}, {0x80892968, 0x024}, {0x808928A0, 0x0C8}, {0x80890B38, 0x1BF8},
    {0x808909C0, 0x178}, {0x80890910, 0x0B0}, {0x80892AE4, 0x0BC}, {0x80892750, 0x150},
    {0x80892730, 0x020}, {0x8088F368, 0x15A8},{0x8088EE18, 0x550}, {0x803537E4, 0x2AC},
    {0x803535C8, 0x21C}, {0x80892BAC, 0x014}, {0x80892F8C, 0x2F4}, {0x80893280, 0x080},
    {0x80893300, 0x00E}, {0x80893310, 0x004}, {0x80893314, 0x006},
    {0x80892684, 0x020},   // RNG state (snapshot omits it)
    {0x8088A808, 0x008},   // playFrameCounter + lastPlayFrameCounter
};

static void seed_copy(int to_scratch)
{
    u32 off = SCRATCH;
    for (int i = 0; i < N_SEED; i++) {
        u32 live = SEED[i].addr, n = SEED[i].size;
        for (u32 j = 0; j < n; j++) {
            if (to_scratch) *((u8*)(off + j)) = *((u8*)(live + j));
            else            *((u8*)(live + j)) = *((u8*)(off + j));
        }
        off += n;
    }
}

static u32 band_checksum(void)
{
    u32 h = 0;
    for (u32 a = HASH_LO; a < HASH_HI; a += 4)
        h = (h * 33) + *((u32*)a);
    return h;
}

CGECKO(RollbackSeedTest, .address = 0x80699D34, .state = MSSB_GAME);
void RollbackSeedTest()
{
    RT_HEART = RT_HEART + 1;   // proves the hook is running at all

    if (g_GameLogic.sceneID != SCENE_ID_LIVE_BALL) { RT_MODE = 0; return; }

    if (RT_MODE == 0) {
        if ((g_InputBuffer.pads[0].button & INPUT_TRIGGER_Z) == INPUT_TRIGGER_Z)
            RT_MODE = 1;
    } else if (RT_MODE == 1) {
        seed_copy(1);
        RT_MODE = 2;
    } else if (RT_MODE == 2) {
        RT_HASHA = band_checksum();
        seed_copy(0);
        RT_MODE = 3;
    } else if (RT_MODE == 3) {
        RT_HASHB = band_checksum();
        RT_RESULT = (RT_HASHB == RT_HASHA) ? 1 : 2;
        RT_MODE = 0;
    }
}

/*-----------------------------------------------------------------------------
 HOW TO READ IT (Dolphin memory viewer, or savestate slot 1 for me to read):
 - HEARTBEAT climbing every frame  => the C2 hook works. If it stays 0, the hook
   itself is broken (crash is the injection, not the test).
 - Hold Z during a live ball: MODE steps 1->2->3->0 over three frames.
 - RESULT 1 (COMPLETE) => the 18 KB seed reproduced the frame -> Path B works.
   RESULT 2 (INCOMPLETE) with HASH_A != HASH_B => seed is missing state; widen
   SEED[] (or the derived heap) and re-test.
 - If it still crashes with the heartbeat climbing, the crash is seed_copy or the
   restore -> that itself is the finding (naive struct-copy rewind isn't safe
   mid-play; the derived actor/animation heap holds live references).
-----------------------------------------------------------------------------*/
