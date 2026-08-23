/*###########################################################
# Boot Directly To Game
###########################################################*/
// Author: LittleCoaks
//
// Boots straight into a match from the title screen.
//
// HOW IT WORKS: rather than force-swapping the rel (which lands the game rel on
// top of a menu that was never torn down -- the long-standing DSI at
// 0x800B4910), it drives the REAL menus: it writes each screen's selections
// into the same addresses Project Rio's "fast reset from HUD" feature uses
// (Source/Core/Core/MSB_GenerateQuickMatchSetupGeckoCode.{h,cpp}) and pulses
// the A button to confirm each screen. The game therefore reaches its own
// start-match path and the DOL performs its normal teardown.
//
// DESIGN: the match is described entirely by `boot_spec` and `boot_config`
// below. The walk itself contains no policy -- no randomisation, no defaults,
// no calls to the game's own auto-draft or selectRandomStadium -- so turning
// this into a "random teams" mod means filling the spec from a generator and
// touching nothing else.
//
// Values are re-written EVERY frame while their screen is up, because each
// screen initialises its own state when it loads and would otherwise clobber a
// one-shot write. This mirrors how Rio's gecko write-codes behave.
//
// The four hooks are at the bottom of the file.

// TODO(decomp-migration): Menu data came from the Ghidra-exported MenuData.h.
// Replace with the decomp headers under Include/game/ and remap the
// symbol names -- see Include/_sync_report.txt and the decomp's
// docs/ghidra_name_conflicts.md.
#include "Include/game/UnknownHomes_Game.h"

/* =========================================================================
   Caller-supplied description of the match to boot into.
   Rosters and per-player arrays are in POSITION order:
     0=P 1=C 2=1B 3=2B 4=3B 5=SS 6=LF 7=CF 8=RF
   (same convention as Rio's MSBQuickMatchGameState, so generators can be
   shared between the two.)
   ========================================================================= */
typedef struct BootMatchSpec
{
    u8 captain[2];            // charID of each team's captain
    u8 roster[2][9];          // charIDs, position order
    u8 battingHand[2][9];     // 0 = right, 1 = left
    u8 fieldingHand[2][9];    // 0 = right, 1 = left
    u8 superstar[2][9];       // 0 = off, 1 = on
    u8 captainOrderLoc[2];    // captain's slot in the batting order, 0-8
    u8 logo[2];               // team logo id, 0-0x2F

    u8 stadiumCursor;         // stadium-select CURSOR index, not the in-game
                                // stadium id: 0=Mario 1=Bowser 2=Wario
                                // 3=Yoshi 4=Peach 5=DK. Toy Field is NOT
                                // reachable from this screen -- a Toy Field
                                // boot needs a different path, unverified.
    u8 firstBatter;           // 0 = P1 bats first, 1 = P2
    u8 starSkills;            // 0 = off, 1 = on
    u8 innings;               // actual inning count; only odd values are
                                // representable (the menu stores a cursor
                                // index), evens land on value-1
    u8 mercy;                 // 0 = off, 1 = on

    /* ---- who works the menus ---- */
    u8 isCpuMatch;            // 1 = P1 vs CPU. Only player 1 presses A; the
                                //     second menu player is never registered.
    u8 p2Port;                // physical controller port for the second
                                // human, 2-4 (1-based, as the player sees it).
                                // Ignored when isCpuMatch. P1 is always port 1.
} BootMatchSpec;

/* =========================================================================
   Where in a game to land once the match starts. Applied every pre-pitch
   frame because game init overwrites these values partway through the load.
   ========================================================================= */
typedef struct BootStateConfig
{
    u8 apply_state;        // 0 = fresh match; 1 = jump to the state below
    u8 inning;             // current inning, 1-based. KEEP IT <= the spec's
                             // `innings`: a value past regulation puts the
                             // match in extras from the first at-bat, and the
                             // game's BGM selection (urgencyByInning,
                             // 0x8089294C) then asks for music the loader
                             // never staged -- the match plays silent until it
                             // re-selects on the next batter.
    u8 bottom_of_inning;   // 0 = top, 1 = bottom
    u16  score_away;         // total runs (also written to inning 1's box)
    u16  score_home;
    u8 balls;              // 0-3
    u8 strikes;            // 0-2
    u8 outs;               // 0-2
    u8 stars_p1;           // 0-5
    u8 stars_p2;
    u8 star_chance;        // 0 = off, 1 = star chance active
} BootStateConfig;

/* =========================================================================
   THE MATCH TO BOOT INTO.

   Everything the walk needs comes from here -- it holds no defaults and never
   asks the game to pick anything.

   Rosters are in POSITION order: P, C, 1B, 2B, 3B, SS, LF, CF, RF.
   ========================================================================= */
static const BootMatchSpec boot_spec =
{
    // Captains deliberately are NOT roster slot 0, so a captain field that is
    // being ignored (or silently taken from the roster) is obvious on screen.
    .captain = { CHAR_ID_BOWSER, CHAR_ID_PEACH },
    .roster = {
        { CHAR_ID_BOWSER, CHAR_ID_GOOMBA, CHAR_ID_DRYBONES_RED, CHAR_ID_BIRDO, CHAR_ID_TOAD_BLUE, CHAR_ID_LUIGI, CHAR_ID_KOOPA_RED, CHAR_ID_PARATROOPA_RED, CHAR_ID_MAGIKOOPA_BLUE },
        { CHAR_ID_PEACH, CHAR_ID_DAISY, CHAR_ID_TOADETTE, CHAR_ID_NOKI_BLUE, CHAR_ID_PIANTA_RED, CHAR_ID_YOSHI, CHAR_ID_MONTY, CHAR_ID_SHYGUY_RED, CHAR_ID_BOO },
    },
    // Every field below is deliberately set AWAY from its default so each one
    // is individually confirmable on screen. Expected result:
    //   left-handed: P1 slots 1,3,5 and P2 slots 0,2 (rest right)
    //   superstars:  P1 slots 0,4 and P2 slots 1,8
    //   Yoshi Park, P2 bats first, star skills OFF, 5 innings, mercy ON
    .battingHand      = { { 0,1,0,1,0,1,0,0,0 }, { 1,0,1,0,0,0,0,0,0 } },
    .fieldingHand     = { { 0,1,0,1,0,1,0,0,0 }, { 1,0,1,0,0,0,0,0,0 } },
    .superstar        = { { 1,0,0,0,1,0,0,0,0 }, { 0,1,0,0,0,0,0,0,1 } },
    .captainOrderLoc  = { 3, 6 },   // captains bat 4th (P1) and 7th (P2)
    .logo             = { 5, 12 },
    .stadiumCursor    = 3,      // 0=Mario 1=Bowser 2=Wario 3=Yoshi 4=Peach 5=DK
    .firstBatter      = 1,      // P2 bats first (flips away/home vs default)
    .starSkills       = 1,      // OFF (default is on)
    .innings          = 5,      // odd values only (menu stores a cursor index)
    .mercy            = 1,      // ON

    .isCpuMatch       = 0,      // two humans: both must confirm the menus
    .p2Port           = 2,      // second human on controller port 2
};

/* ---- boot-to-state: the exact point in the game to land on ------------- */

// PLACEHOLDER: a very recognizable mid-game state -- bottom 5th, away up 3-2,
// 2-1 count, 2 outs, P1 4 stars / P2 1 star. Every field is HUD-visible.
static const BootStateConfig boot_config =
{
    // Also all set away from defaults, and every one is HUD-visible:
    // top of the 7th, away 8 - home 3, full count, 2 outs, 5 vs 2 stars,
    // star chance lit.
    .apply_state      = 1,
    // MUST be <= the spec's `innings`. This was 7 against a 5-inning game,
    // which drops the match into EXTRAS on the very first at-bat -- the game
    // then wants extras BGM that the loader never staged, and no music plays
    // until it re-selects on the next batter. 3 of 5 is late but in regulation.
    .inning           = 3,
    .bottom_of_inning = 0,      // TOP of the inning (was bottom)
    .score_away       = 8,
    .score_home       = 3,
    .balls            = 3,      // full count -- one more ball walks him
    .strikes          = 2,
    .outs             = 2,      // one out from retiring the side
    .stars_p1         = 5,      // max
    .stars_p2         = 2,
    .star_chance      = 0,
};

/* ---- menu addresses -----------------------------------------------------
   Everything here comes from the generated headers rather than a literal
   address; the aliases exist only so the code below reads as "the thing the
   boot walk sets". The set matches Rio's quick-match builder.

   Handedness and the superstar flag live in the in-memory roster
   (inMemRoster, 0xA0 per player, team-then-batting-order) -- the menus
   normally fill them from character defaults as the team is drafted.
   The superstar flag sits in Stats.UnusedBytes[0], which is unused as far as
   the stat tables are concerned but is what the star routine reads. */
#define btm_captainChar     Static_Stats_Tables.captainSelectedID
#define btm_captainOrderLoc Static_Stats_Tables.capLocationInOrder_P1_P2_
#define btm_logoAwayHome    Static_Stats_Tables.teamName_P1_P2_
#define btm_charsP1         Roster_Player1.rosterCharID[0]
#define btm_charsP2         Roster_Player1.rosterCharID[1]
#define btm_spotFilledP1    Roster_Player1.rosterSpotFilledInd[0]
#define btm_spotFilledP2    Roster_Player1.rosterSpotFilledInd[1]
#define btm_okActiveP1      okButtonSelectable[0]
#define btm_okActiveP2      okButtonSelectable[1]
#define btm_cursorP1        positionCursorPosition[0]
#define btm_cursorP2        positionCursorPosition[1]
// the cursor is a u32; only its low u8 (0x80750C37) is ever non-zero
#define btm_stadiumCursor   cursorPos_stadSelect
#define btm_firstBatter     VAR_ADDRESS(u8, firstBattingTeam_ADDR)
#define btm_starSkills      VAR_ADDRESS(u8, starSkills_ADDR)
#define btm_inningsCursor   VAR_ADDRESS(u8, inningSelectionOnMenu_ADDR)
#define btm_mercy           VAR_ADDRESS(u8, mercyRule_ADDR)
#define btm_superstarFlag(team, i) (inMemRoster[team][i].Stats.UnusedBytes[0])
#define BTM_CHAR_SELECT_OK  9            // cursor value that sits on OK

/* ---- engine state (claimed free memory, see ClaimedFreeMemory.h) -------- */
#define btm_armed      VAR_ADDRESS(u8, 0x802EBF9B)  // 0xFF once armed
#define btm_frame      VAR_ADDRESS(u8, 0x802EC017)  // A-pulse clock
#define btm_done       VAR_ADDRESS(u8, 0x802EC018)  // walk finished, hands off
#define btm_heartbeat  VAR_ADDRESS(u8, 0x802EC016)  // liveness for the boot test
#define btm_settle     VAR_ADDRESS(u8, 0x802EBF90)  // pre-arm menu settle count
#define btm_muted      VAR_ADDRESS(u8, 0x802EC019)  // audio muted by the walk
// per-team superstar walk progress (the two bytes Auto-Superstar.asm claims)
#define btm_ssIndex    ARRAY_1D_ADDRESS(u8, 2, 0x802EBF99)
#define btm_stateFrames VAR_ADDRESS(u8, 0x802EC01A) // game-state burst counter
// The game's own audio mute/unmute helpers. They wrap sndMasterVolume with
// exactly (0, 1, 1, 1) and (127, 1, 1, 1) -- note time=1, not 0. Calling these
// rather than sndMasterVolume directly keeps our behaviour identical to the
// game's, so nothing is left half-faded.
#define btm_audioUnmute() (((void(*)(void))0x80021188)())
#define btm_audioMute()   (((void(*)(void))0x800211BC)())
// 1 = silence the menu walk (costs the first at-bat's BGM, see the note in
// BootToMatch_Step). 0 = let the menus be audible for the few seconds they run,
// which keeps match music correct.
#define BTM_MUTE_AUDIO 1
// Bisect for the first-at-bat BGM silence. 1 = apply inning/halfInning/batting
// team during the load (full behaviour); 0 = skip that group and apply only
// scores, count and stars. See BootToMatch_ApplyGameState.
// The outs are applied a short while AFTER the match starts, not during the
// load -- see BootToMatch_ApplyGameState. Delay long enough for the game's
// match-start music to get going, then hold them on for a few frames so the
// write is not lost to init.
// 20 was NOT enough -- the BGM had still not started by then and the music was
// lost exactly as when the outs were applied during the load. 180 frames (~3s)
// puts the write well after the match music is up and running.
#define BTM_OUTS_DELAY_FRAMES 180
#define BTM_OUTS_BURST_FRAMES 10
// How long to wait after `rel` first reads 4 before touching the menu. The
// counter starts while the menu rel is still loading, so this is "let it come
// up", not a precise delay.
#define BTM_SETTLE_FRAMES 60

/* Captain select, two-human flow:
     P1 presses A -> picks its own captain
     P1 presses A again -> picks the CPU's captain, and the DIFFICULTY screen
       appears, locking the match to P1 vs CPU
     another port pressing A BEFORE that second press takes the slot over;
       that port then needs one press to take control and a second to pick
   Two players both mashing A therefore race, and P1 usually wins -- the walk
   ends up stranded on the CPU difficulty screen. Rio's fast reset hits the
   same problem and solves it by branching over the "P1 grabs the CPU captain"
   path, which holds the slot open until the second port claims it. */
#define BTM_CAPTAIN_PREVENT_CPU_ADDR  0x806548DC
#define BTM_CAPTAIN_PREVENT_CPU_PATCH 0x48000234   // b +0x234
#define BTM_CAPTAIN_PREVENT_CPU_ORIG  0x38E70154   // addi r7,r7,0x154
#define BTM_PULSE_PERIOD 4   // menus latch the rising edge of newlyPressed,
                              // so a held button does nothing after frame 1
// Bisect switch: 0 = walk the menus but write no selections (whatever the A
// presses happen to pick is what you get). Used to tell "our data writes are
// corrupting something" from "the boot itself is broken".
#define BTM_APPLY_DATA 1
// MEASURED 0, twice (2026-07-20): injecting menu input from main.dol does NOT
// work -- at 0x80009404 or 0x80009450, with or without the raw pad table and
// per-port writes, the menu never leaves screenCode 5. The rel's own process
// refreshes the pad state before its gather, so main.dol writes are always
// overwritten. The 0x8063F788 menu-rel injection cannot be removed.
#define BTM_INPUT_FROM_DOL 0

// menu input plumbing: the menu rel rebuilds Static_Stats_Tables.controllerInputs
// from the global pad table each frame (gather 0x8063F6F8-0x8063F794), so
// injection has to land after that copy -- nothing written from main.dol
// survives (measured). The gather walks 6-u8 entries with r7 starting at
// Static_Stats_Tables, so the player index is (r7 - that) / 6.
#define btm_menuInputs    Static_Stats_Tables.controllerInputs
#define BTM_GATHER_ENTRY0 ((u32)&Static_Stats_Tables)
// global pad table: halfwords +0 held / +2 newlyPressed / +4 processed, one
// 32-u8 entry per port. No struct label for it in the headers.
#define BTM_PAD_TABLE  ((u32)&AtBat_ButtonInput1)
#define BTM_PAD_STRIDE 32
// index = menu player, value = pad port (0-based); the name reads backwards
#define btm_portForPlayer Static_Stats_Tables.playerNumberByPort

/* =========================================================================
   Arm the engine. Call once, from the menu, when you want the boot to begin.
   ========================================================================= */
static inline void BootToMatch_Arm(void)
{
    btm_armed = 0xFF;
    btm_frame = 0;
    btm_done  = 0;
    btm_stateFrames = 0;
    btm_ssIndex[0] = 0;   // restart the superstar walk for this boot
    btm_ssIndex[1] = 0;
}

static inline int BootToMatch_IsArmed(void) { return btm_armed == 0xFF && btm_done == 0; }

// Blank video / silence audio only while the walk is actually on the menus.
// Once it leaves rel 4 the loading screen and match render normally.
static inline int BootToMatch_Hiding(void) { return BootToMatch_IsArmed() && inningSetting.rel == 4; }

/* =========================================================================
   Per-frame menu-input injection. Call from a code injected at 0x8063F788 with
   .state = MSSB_MENU -- that is the gather loop's common continuation, reached both
   by the normal copy path and by the "port maps to -1, zero it" path, so it
   still runs with no controller plugged in.
   ========================================================================= */
static inline void BootToMatch_MenuInput(const BootMatchSpec* s)
{
    if (!BootToMatch_IsArmed())
        return;

    // The gather loops once per MENU PLAYER (not per port), r7 walking 6-u8
    // controllerInputs_ entries from 0x8034E9A0; entry i is fed from pad port
    // playerNumberByPort_[i]. So player index = (r7 - base) / 6.
    READ_GAME_REG(u32, gather_entry, 7);
    int player = (int)((gather_entry - BTM_GATHER_ENTRY0) / 6);
    if (player < 0 || player > 1)
        return;

    // A two-human match needs BOTH players to confirm the captain and roster
    // screens -- pressing A as player 1 alone stalls on roster select forever.
    // In a CPU match only player 1 exists and player 2 must stay silent.
    if (player == 1 && s->isCpuMatch)
        return;

    if (player == 0)            // tick the clock once per frame, not per player
        btm_frame++;

    // Stagger the two players by half a period. Pressing on the same frame
    // makes every shared prompt a coin flip; offsetting them behaves like two
    // people mashing independently and gives player 2 a clean frame to claim
    // the captain slot.
    u8 phase = (player == 0) ? 0 : (BTM_PULSE_PERIOD / 2);
    short press = (btm_frame % BTM_PULSE_PERIOD) == phase ? INPUT_BUTTON_A : 0;
    btm_menuInputs[player].currentHeldInput = press;
    btm_menuInputs[player].newInput         = press;
    btm_menuInputs[player].processedInput   = press;

    // Also drive the RAW pad table for this player's port. "Another port takes
    // over the second captain slot" is detected by watching ports that are not
    // yet assigned to a menu player, which reads the pad table directly rather
    // than the gathered controllerInputs_ copy -- writing only the copy leaves
    // the walk parked on captain select forever.
    u8 port = (player == 0) ? 0 : (u8)(s->p2Port - 1);
    short* pad = (short*)(BTM_PAD_TABLE + port * BTM_PAD_STRIDE);
    pad[0] = press;   // held
    pad[1] = press;   // newlyPressed
    pad[2] = press;   // processed
}

/* ---- per-screen data application ---------------------------------------- */
static inline void BootToMatch_ApplyCaptains(const BootMatchSpec* s)
{
    btm_captainChar[0] = s->captain[0];
    btm_captainChar[1] = s->captain[1];
    btm_logoAwayHome[0] = s->logo[0];
    btm_logoAwayHome[1] = s->logo[1];
}

static inline void BootToMatch_ApplyRosters(const BootMatchSpec* s)
{
    for (int i = 0; i < 9; i++)
    {
        btm_charsP1[i] = s->roster[0][i];
        btm_charsP2[i] = s->roster[1][i];
        btm_spotFilledP1[i] = 1;
        btm_spotFilledP2[i] = 1;
    }
    // let both sides confirm, and park the cursors on OK
    btm_okActiveP1 = 1;
    btm_okActiveP2 = 1;
    btm_cursorP1 = BTM_CHAR_SELECT_OK;
    btm_cursorP2 = BTM_CHAR_SELECT_OK;
}

static inline void BootToMatch_ApplyPlayerFlags(const BootMatchSpec* s)
{
    for (int team = 0; team < 2; team++)
    {
        for (int i = 0; i < 9; i++)
        {
            CharacterStats* p = &inMemRoster[team][i];
            p->Stats.FieldingArm    = s->fieldingHand[team][i];
            p->Stats.BattingStance  = s->battingHand[team][i];
            p->Stats.UnusedBytes[0] = s->superstar[team][i];
        }
    }
    btm_captainOrderLoc[0] = s->captainOrderLoc[0];
    btm_captainOrderLoc[1] = s->captainOrderLoc[1];
}

static inline void BootToMatch_ApplySettings(const BootMatchSpec* s)
{
    btm_stadiumCursor = s->stadiumCursor;
    btm_firstBatter   = s->firstBatter;
    btm_starSkills    = s->starSkills;
    // the screen stores a cursor index, not the inning count
    btm_inningsCursor = (u8)((s->innings - 1) >> 1);
    btm_mercy         = s->mercy;
}

/* =========================================================================
   Per-frame driver. Call every frame from a main.dol hook. Writes the
   selections for whichever screen is currently up; the A pulse injected by
   BootToMatch_MenuInput() confirms each one and the menus advance on their own.
   ========================================================================= */
static inline void BootToMatch_Step(const BootMatchSpec* s)
{
    if (btm_armed != 0xFF || btm_done)
        return;
    if (inningSetting.rel != 4)
    {
        // left the menu: the walk did its job. Latch off so returning to the
        // menus later doesn't silently start booting another match.
        if (inningSetting.rel == 5)
            btm_done = 1;
        return;
    }

    // Hold the second captain slot open for the other port (see the comment on
    // BTM_CAPTAIN_PREVENT_CPU_ADDR). Menu-rel code, so this is only safe here,
    // at rel == 4, and it does not need restoring -- the rel is reloaded from
    // disc on the next visit.
    if (!s->isCpuMatch)
        *(u32*)BTM_CAPTAIN_PREVENT_CPU_ADDR = BTM_CAPTAIN_PREVENT_CPU_PATCH;

    // Register who is playing. Index is the MENU PLAYER, value is the pad port
    // (0-based) -- despite the name. P1 is always port 1; the second human sits
    // on port 2-4 so the menus see a real second controller and let them join.
    btm_portForPlayer[0] = 0;
    if (!s->isCpuMatch)
        btm_portForPlayer[1] = (u8)(s->p2Port - 1);

    // Apply every selection every frame, on every screen -- exactly what Rio's
    // fast-reset write-codes do. Whatever the (spammed) A presses happen to
    // pick gets overwritten before it matters, so no per-screen logic is
    // needed and no screen can be missed while it is still initialising.
#if BTM_APPLY_DATA
    // Which sub-menu we are on is decided HERE in C, not by the injection --
    // the injection is declared for the menu rel (rel 4), which it must be,
    // because that controls whether the branch is written into menu-rel code
    // at all.
    // Each screen's values are re-applied every frame it is up, since screens
    // initialise their own state on load and would clobber a one-shot write.
    if (menuControlVariables != 0)
    {
        switch (menuControlVariables->screenCode)
        {
        case enumMenuScreen_captainSelect:
            BootToMatch_ApplyCaptains(s);
            break;
        case enumMenuScreen_rosterSelect:
            BootToMatch_ApplyCaptains(s);
            BootToMatch_ApplyRosters(s);
            break;
        case enumMenuScreen_battingLineup:
            BootToMatch_ApplyRosters(s);
            BootToMatch_ApplyPlayerFlags(s);
            break;
        case enumMenuScreen_stadiumSelect_challengeChooseDifficulty:
        case enumMenuScreen_matchSettings:
            BootToMatch_ApplyPlayerFlags(s);
            BootToMatch_ApplySettings(s);
            break;
        default:
            break;
        }

        // Give audio back on the LAST menu screen rather than at the rel swap,
        // so the menu->match music handoff happens with the sound engine in its
        // normal state. Muting across that handoff is what leaves the first
        // at-bat's BGM silent -- the group volumes are provably correct either
        // way (traced live), so it is the handoff, not the volume. Video stays
        // blanked until the rel actually changes.
        // Kept OUT of the switch on purpose: duplicating a case body made GCC
        // inline these apply-loops twice and the payload jumped 228 -> 612
        // words, which hung the game.
        if (btm_muted &&
            menuControlVariables->screenCode >= enumMenuScreen_stadiumSelect_challengeChooseDifficulty)
        {
            btm_audioUnmute();
            btm_muted = 0;
        }
    }
#endif

    // Silence the walk. ONE-SHOT: re-issuing the mute every frame re-triggers a
    // fade and fights the game's own audio transitions. Uses the game's own
    // mute/unmute pair so the parameters (0/127, time=1) match what it does to
    // itself.
    //
    // KNOWN ISSUE (2026-07-20): with muting on, the match BGM is silent for the
    // first at-bat and only comes in afterwards; FX are fine throughout. It is
    // NOT the group volumes -- those were traced live and are correct the whole
    // time (music group float 0x8030EBCC: 1.0 -> 0.0 on mute -> 1.0 on unmute,
    // and 1.0 across the first at-bat). So the volume is up and the match music
    // sequence simply is not playing, i.e. muting suppresses the BGM START
    // rather than the BGM output. Set BTM_MUTE_AUDIO to 0 to confirm/avoid.
#if BTM_MUTE_AUDIO
    if (!btm_muted)
    {
        btm_audioMute();
        btm_muted = 1;
    }
#endif
}

/* =========================================================================
   Land the match on a specific game state. Call every frame once in game.
   ========================================================================= */
static inline void BootToMatch_ApplyGameState(const BootStateConfig* c)
{
    if (!c->apply_state || inningSetting.rel != 5)
        return;

    // hasGameStarted_ is NOT "first pitch thrown" -- measured 2026-07-20, it
    // flips to 1 on the same frame GameStatus becomes AtBat. It marks the match
    // beginning, which splits this into two phases.
    if (g_GameLogic.EventTriggers_GameHasStarted == 0)
    {
        /* ---- during the load: everything EXCEPT the outs ---- */
        // Game init overwrites these partway through, so re-apply every frame.
        g_Scores.Inning     = c->inning;
        g_Scores.halfInning = c->bottom_of_inning;
        g_GameLogic.homeTeamBattingInd_fieldingTeam  = c->bottom_of_inning ? 1 : 0;
        g_GameLogic.awayTeamBattingInd_battingTeam = c->bottom_of_inning ? 0 : 1;

        g_Scores.scores[0].total       = c->score_away;
        g_Scores.scores[0].byInning[0] = c->score_away;  // book all runs in the 1st
        g_Scores.scores[1].total       = c->score_home;
        g_Scores.scores[1].byInning[0] = c->score_home;

        g_Strikes.balls   = c->balls;
        g_Strikes.strikes = c->strikes;

        g_GameLogic.TeamStars[0] = c->stars_p1;
        g_GameLogic.TeamStars[1] = c->stars_p2;
        g_GameLogic.IsStarChance = c->star_chance;
        return;
    }

    /* ---- after the match begins: the outs, and only now ----
       Booting with outs already on the board is what silenced the first
       at-bat's BGM -- bisected field by field 2026-07-20, everything else in
       this struct is innocent (and so were the walk's audio mute, the group
       volumes and the load-time timing). The match-start path evidently picks
       its music for a normal 0-out first at-bat and gives up if the outs are
       already set, which is why pausing or a new batter brought it back.
       So let the match start clean, then put the outs on a few frames later. */
    if (btm_stateFrames >= BTM_OUTS_DELAY_FRAMES + BTM_OUTS_BURST_FRAMES)
        return;                                  // done; hands off the match
    btm_stateFrames++;
    if (btm_stateFrames <= BTM_OUTS_DELAY_FRAMES)
        return;                                  // let the BGM get going first

    // The delay means this lands on a live match, so never overwrite the outs
    // once a pitch is actually in flight -- that would be rewriting gameplay
    // state mid-play rather than setting up the boot. In practice the batter
    // walk-up covers the delay, so this only trips if the player pitches
    // immediately, in which case the outs are simply left alone.
    if (AtBat_PitchThrown != 0)
    {
        btm_stateFrames = BTM_OUTS_DELAY_FRAMES + BTM_OUTS_BURST_FRAMES;  // give up
        return;
    }

    g_Strikes.outs       = c->outs;
    g_Strikes.storedOuts = c->outs;
}

/* =========================================================================
   The whole main.dol side, so the hook body below is one call.
   ========================================================================= */
static inline void BootToMatch_MainHook(const BootMatchSpec* s, const BootStateConfig* c)
{
    // Heartbeat: this runs every frame in every rel state while the CPU is
    // alive. It is the only trustworthy liveness signal for the live boot test
    // -- sampling RAM for churn reports an idle menu as a halted CPU.
    btm_heartbeat++;

    if (inningSetting.rel == 4 && btm_armed != 0xFF)
    {
        // let the menu finish loading before touching it
        if (btm_settle < BTM_SETTLE_FRAMES) { btm_settle++; return; }
        BootToMatch_Arm();
        return;
    }

    BootToMatch_Step(s);

#if BTM_INPUT_FROM_DOL
    // EXPERIMENT: drive the pad table from main.dol instead of the menu-rel
    // hook. If this walks the menus, the 0x8063F788 injection (and the whole
    // menu-rel-only complication) can be deleted.
    if (BootToMatch_IsArmed() && inningSetting.rel == 4)
    {
        btm_frame++;
        for (int player = 0; player < (s->isCpuMatch ? 1 : 2); player++)
        {
            u8 phase = (player == 0) ? 0 : (BTM_PULSE_PERIOD / 2);
            short press = (btm_frame % BTM_PULSE_PERIOD) == phase
                        ? INPUT_BUTTON_A : 0;
            u8 port = (player == 0) ? 0 : (u8)(s->p2Port - 1);
            short* pad = (short*)(BTM_PAD_TABLE + port * BTM_PAD_STRIDE);
            pad[0] = press; pad[1] = press; pad[2] = press;
            btm_menuInputs[player].currentHeldInput = press;
            btm_menuInputs[player].newInput         = press;
            btm_menuInputs[player].processedInput   = press;
        }
    }
#endif

    // Restore audio the moment the walk stops hiding, exactly once. Without
    // this the mute set during the walk would follow us into the match and it
    // would play silently forever.
    if (btm_muted && !BootToMatch_Hiding())
    {
        btm_audioUnmute();
        btm_muted = 0;
    }

    BootToMatch_ApplyGameState(c);
}

/* =========================================================================
   Superstars. Writing the per-player bools is NOT enough on its own -- the
   game only stars whoever the team-management cursor is sitting on, so it has
   to be walked across the roster one entry per pass. This is the same trick as
   the repo's Menu/Auto-Superstar.asm (Nuche17) and Rio's SUPERSTAR_INJECTION:
   hook 0x8005A4F4 (r27 = team) and step a 1-indexed counter 1..9 over the
   roster, parking the cursor on each spot and copying that spot's bool into
   InProgress_superStarAPlayer, which triggers the star.
   ========================================================================= */
static inline void BootToMatch_AutoSuperstar(void)
{
    if (!BootToMatch_IsArmed())     // never touch a normally-played menu
        return;

    READ_GAME_REG(int, team, 27);
    if (team < 0 || team > 1)
        return;

    u8 i = btm_ssIndex[team];
    if (i == 0)                     // let the bools settle for one pass first
    {
        btm_ssIndex[team] = 1;
        return;
    }
    if (i > 0xA)                    // done
        return;

    if (i == 0xA)                   // finished the roster: park the cursor
    {
        teamManagement_cursorPos[team] = 0;
        btm_ssIndex[team] = i + 1;
        return;
    }

    // counter is 1-indexed, roster slots are 0-indexed
    teamManagement_cursorPos[team] = i;
    InProgress_superStarAPlayer[team] = btm_superstarFlag(team, i - 1);
    btm_ssIndex[team] = i + 1;
}

/* =========================================================================
   Graphics blanking. The menu walk is meant to look like a black screen, so
   both projection matrices are zeroed while it runs, collapsing every 3D and
   2D draw to nothing. This is the same pair of hook sites Widescreen.c uses:
     0x800901F0 -- GXSetProjection  (3D + ortho layers)
     0x80090284 -- GXSetProjectionv (all screen-space 2D: menus, text, HUD)
   both immediately after the values land in the GX state block, with r5
   holding that block. Nothing needs restoring: the game re-sends projections
   every frame, so normal rendering resumes the moment this stops firing.
   ========================================================================= */
static inline void BootToMatch_HideProjection(void)
{
    READ_GAME_REG(u8*, gx, 5);
    if (!BootToMatch_Hiding())
        return;
    VAR_ADDRESS(float, (u32)gx + 0x4DC) = 0.0f;   // m[0][0]
    VAR_ADDRESS(float, (u32)gx + 0x4E4) = 0.0f;   // m[1][1]
}

/* =========================================================================
   Hooks. Every body stays a one-liner over the code above.
   ========================================================================= */
CGECKO(BootDirectlyToGame, .address = 0x80009404,
                           .instruction = "mr r3, r25");
void BootDirectlyToGame()
{
    BootToMatch_MainHook(&boot_spec, &boot_config);
}

/* Menu input injection. MENU REL, hence this code's own .state = MSSB_MENU
   -- at inningSetting.rel 5 the same address holds game-inningSetting.rel code. 0x8063F788 is the
   gather loop's common continuation, reached both by the normal copy path and
   by the "port maps to -1, zero it" path, so it runs with no controller
   plugged in. Injecting from main.dol does not work: the inningSetting.rel's own process
   refreshes the pad state before its gather, so nothing written there
   survives (measured at 0x80009404 and 0x80009450). */
CGECKO(BootMenuInput, .address = 0x8063F788, .state = MSSB_MENU,
                      .instruction = "addi r5, r5, 1");
void BootMenuInput()
{
    BootToMatch_MenuInput(&boot_spec);
}

/* Superstars: the per-player bools only take effect if the team-management
   cursor is walked across the roster -- see BootToMatch_AutoSuperstar. */
CGECKO(BootAutoSuperstar, .address = 0x8005A4F4,
                          .instruction = "lis r3, 0x8033");
void BootAutoSuperstar()
{
    BootToMatch_AutoSuperstar();
}

/* Blank the screen while the walk runs -- see BootToMatch_HideProjection. */
CGECKO(BootHideProjection, .address = 0x800901F0,
                           .instruction = "lis r5, -13311");
void BootHideProjection()
{
    BootToMatch_HideProjection();
}

CGECKO(BootHideProjectionV, .address = 0x80090284,
                            .instruction = "lis r4, -13311");
void BootHideProjectionV()
{
    BootToMatch_HideProjection();
}
