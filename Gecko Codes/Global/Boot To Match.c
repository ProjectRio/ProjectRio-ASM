/*###########################################################
# Test New Boot Match
###########################################################*/
// Author: LittleCoaks
// *Boots directly to a Predetermined match. Used in Fast Reset.

// TODO(decomp-migration): Menu data came from the Ghidra-exported MenuData.h.
// Replace with the decomp headers under Include/game/ and remap the
// symbol names -- see Include/_sync_report.txt and the decomp's
// docs/ghidra_name_conflicts.md.
#include "Include/game/UnknownHomes_Game.h"

#include "Include/Local/Legacy.h"
#include "Include/Local/LegacyMenus.h"

/* This code runs across the menu -> match handoff, so it touches an object in
   the match REL while bound to the menu REL's symbols. Include/Symbols refuses
   to bind both at once (they share an arena slot), which is right for a normal
   mod -- so g_Scores is bound here by address instead, and is only valid once
   the match REL is actually resident. */
#define g_Scores VAR_ADDRESS(GameScoresControlsStruct, 0x808928A0)
/* =========================================================================
   Caller-supplied description of the match to boot into.
   Copied verbatim from Boot Directly To Game.c so specs are interchangeable.
   Rosters and per-player arrays are in POSITION order:
     0=P 1=C 2=1B 3=2B 4=3B 5=SS 6=LF 7=CF 8=RF
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
                                // 3=Yoshi 4=Peach 5=DK. Toy Field is NOT on
                                // this table -- a Toy Field boot needs the
                                // selectRandomStadium toy-field patch instead.
    u8 firstBatter;           // 0 = P1 bats first, 1 = P2. This is who bats
                                // when the match STARTS -- the game makes this
                                // team the in-game AWAY side. For a
                                // bottom-of-inning boot state this must be the
                                // true HOME player (they bat when play
                                // resumes), paired with swapped logo[] slots
                                // so the scoreboard rows stay true.
    u8 starSkills;            // 0 = off, 1 = on
    u8 innings;               // actual inning count
    u8 mercy;                 // 0 = off, 1 = on

    /* ---- who is playing ---- */
    u8 isCpuMatch;            // 1 = P1 vs CPU (only port 1 is a human).
                                //     UNVERIFIED in the force-swap flow -- the
                                //     tested path is two humans (isCpuMatch 0).
    u8 p2Port;                // physical controller port for the second
                                // human, 2-4 (1-based). Ignored when isCpuMatch.
} BootMatchSpec;

/* =========================================================================
   Where in a game to land once the match starts. Applied every pre-pitch
   frame because game init overwrites these values partway through the load.
   Copied verbatim from Boot Directly To Game.c.
   ========================================================================= */
typedef struct BootStateConfig
{
    u8 apply_state;        // 0 = fresh match; 1 = jump to the state below
    u8 inning;             // current inning, 1-based. KEEP IT <= the spec's
                             // `innings` (a value past regulation boots into
                             // extras and the loader never stages that BGM).
    u8 bottom_of_inning;   // 0 = top, 1 = bottom
    u16  score_away;         // total runs (also written to inning 1's box)
    u16  score_home;
    u8 balls;              // 0-3
    u8 strikes;            // 0-2
    u8 outs;               // 0-2
    u8 stars_p1;           // 0-5
    u8 stars_p2;
    u8 star_chance;        // 0 = off, 1 = star chance active

    /* ---- batting order + fielding positions -----------------------------
       The game's characterSelectScreen() (called during staging) only builds
       a DEFAULT order from the position-ordered roster, so the actual batting
       order, the fielder at each position, and the current batter are all
       wrong for a mid-inning boot. These arrays override the in-game mapping
       (g_GameLogic.battingOrderAndPositionMapping) directly, per the proven
       menu-walk generator. Every array is in AWAY/HOME order (index 0 = away,
       1 = home) and, within a team, CURRENT-BATTER-FIRST: slot 0 is the batter
       who is (or is next) up, slot 1 on deck, and so on. */
    u8 apply_order;           // 0 = leave the game's default order alone
    u8 orderChar_A_H[2][9];   // charID batting in each slot, NATURAL (leadoff-first)
    u8 orderPos_A_H[2][9];    // fielding position in each slot, same ordering

    // Current batter's slot in each team's mapping, 1-indexed (away, home).
    u8 currentBatter_A_H[2];

    // Per NATURAL batting slot, the handedness/superstar of the character there.
    // Indexed [P1/P2][slot] to match the in-memory roster the staging chain
    // builds. Applied after the char hook has reordered inMemRoster, replacing
    // the base character defaults with the HUD's values.
    u8 orderFieldHand_P1P2[2][9]; // 0 = right, 1 = left
    u8 orderBatHand_P1P2[2][9];   // 0 = right, 1 = left
    u8 orderSuperstar_P1P2[2][9]; // 0 = off, 1 = on

    // Runners on base: index 0 = 1B, 1 = 2B, 2 = 3B. runner_present marks a base
    // occupied; runner_rosterLoc is the runner's slot in the batting team's
    // in-memory roster (its natural batting-order index); runner_charID is the
    // runner's character. Placed during the load, with the game's runner-clear
    // instruction nop'd so the runner survives into the live match.
    u8 runner_present[3];
    u8 runner_rosterLoc[3];
    u8 runner_charID[3];

    // positionSwapMapping: the fielding position each roster slot plays, in the
    // game's native draft-slot order (paired with spec.roster, which is now also
    // in draft order). The game builds the on-field defense from these, so they
    // are staged verbatim -- identity does not work because a position-order
    // roster feeds the fielder-table build wrong.
    u8 positionSwap[2][9];
} BootStateConfig;

/* =========================================================================
   THE MATCH TO BOOT INTO — one contiguous, patchable payload.

   The Rio Client (MSB_GenerateQuickMatchSetupGeckoCode.cpp) regenerates this
   gecko code from the HUD file: it scans the compiled code for `magic` and
   overwrites the spec/config bytes that follow with the HUD's match state.
   Because of that:
     - ANY layout change to BootMatchSpec/BootStateConfig/BootMatchPayload must
       be mirrored in the Client's payload offsets, and the Client's embedded
       template must be refreshed from the newly compiled output.
     - every field must be READ AT RUNTIME from this struct. The OPAQUE_PTR
       barrier in the entry point below guarantees that; never access
       boot_payload directly without it, or -O1 folds the value into an
       immediate and the Client's u8-patch silently stops applying.

   Rosters are in POSITION order: P, C, 1B, 2B, 3B, SS, LF, CF, RF.
   (Placeholder values -- every field is set away from its default so it is
   individually confirmable on screen, same convention as Boot Directly.)
   ========================================================================= */
typedef struct BootMatchPayload
{
    u32            magic;      // BOOT_PAYLOAD_MAGIC ('RIOB') — locator for the Client
    BootMatchSpec   spec;       // payload offset 4
    BootStateConfig config;     // payload offset 90 (spec is 85 B + 1 pad for u16 align)
} BootMatchPayload;

#define BOOT_PAYLOAD_MAGIC 0x52494F42  /* 'RIOB' */

static const BootMatchPayload boot_payload =
{
    .magic = BOOT_PAYLOAD_MAGIC,
    .spec =
    {
        .captain = { CHAR_ID_BOWSER, CHAR_ID_PEACH },
        .roster = {
            { CHAR_ID_BOWSER, CHAR_ID_PEACH, CHAR_ID_DRYBONES_RED, CHAR_ID_BIRDO, CHAR_ID_TOAD_BLUE, CHAR_ID_LUIGI, CHAR_ID_KOOPA_RED, CHAR_ID_PARATROOPA_RED, CHAR_ID_MAGIKOOPA_BLUE },
            { CHAR_ID_PEACH, CHAR_ID_PEACH, CHAR_ID_TOADETTE, CHAR_ID_NOKI_BLUE, CHAR_ID_PIANTA_RED, CHAR_ID_YOSHI, CHAR_ID_MONTY, CHAR_ID_SHYGUY_RED, CHAR_ID_BOO },
        },
        .battingHand      = { { 0,1,0,1,0,1,0,0,0 }, { 1,0,1,0,0,0,0,0,0 } },
        .fieldingHand     = { { 0,1,0,1,0,1,0,0,0 }, { 1,0,1,0,0,0,0,0,0 } },
        .superstar        = { { 1,0,0,0,1,0,0,0,0 }, { 0,1,0,0,0,0,0,0,1 } },
        .captainOrderLoc  = { 3, 6 },   // captains bat 4th (P1) and 7th (P2)
        .logo             = { 5, 12 },
        .stadiumCursor    = 3,      // 0=Mario 1=Bowser 2=Wario 3=Yoshi 4=Peach 5=DK
        .firstBatter      = 1,      // P2 bats first
        .starSkills       = 1,      // ON
        .innings          = 5,
        .mercy            = 1,      // ON

        .isCpuMatch       = 0,      // two humans (the tested force-swap path)
        .p2Port           = 2,      // second human on controller port 2
    },

    /* ---- boot-to-state: the exact point in the game to land on ---------- */
    .config =
    {
        .apply_state      = 1,
        // MUST be <= the spec's `innings`.
        .inning           = 3,
        .bottom_of_inning = 0,      // TOP of the inning
        .score_away       = 8,
        .score_home       = 3,
        .balls            = 3,      // full count
        .strikes          = 2,
        .outs             = 2,
        .stars_p1         = 5,
        .stars_p2         = 2,
        .star_chance      = 1,

        // Placeholder batting order (cur-batter-first). Away = P2 here (P2 bats
        // first, top of inning -> P1 is home); home = P1. orderPos is the
        // fielding position batting in each slot; orderChar is that position's
        // roster character. Chosen so each team's captain (P1 pos 0, P2 pos 1)
        // lands at captainOrderLoc {3,6} above, keeping the placeholder
        // self-consistent for the Client's round-trip self-test.
        .apply_order      = 1,
        .orderPos_A_H  = { { 0,2,3,4,5,6,1,7,8 }, { 1,2,3,0,4,5,6,7,8 } },
        .orderChar_A_H = { { 0x04,0x0F,0x18,0x16,0x06,0x12,0x04,0x10,0x0E },
                           { 0x04,0x32,0x11,0x09,0x1D,0x01,0x2A,0x14,0x21 } },
        .currentBatter_A_H = { 1, 1 },
        .orderFieldHand_P1P2 = { { 1,0,1,0,0,1,0,0,0 }, { 1,1,0,0,0,0,0,0,0 } },
        .orderBatHand_P1P2   = { { 1,0,1,0,0,1,0,0,0 }, { 1,1,0,0,0,0,0,0,0 } },
        .orderSuperstar_P1P2 = { { 0,0,0,1,1,0,0,0,0 }, { 0,0,0,0,0,0,1,0,1 } },
        .runner_present   = { 0, 0, 0 },   // no runners in the placeholder
        .runner_rosterLoc = { 0, 0, 0 },
        .runner_charID    = { 0, 0, 0 },
        // placeholder roster is in position order, so positionSwap is identity
        .positionSwap = { { 0,1,2,3,4,5,6,7,8 }, { 0,1,2,3,4,5,6,7,8 } },
    },
};

/* Launder a pointer through an empty asm so GCC forgets it points at const
   data. Without this, -O1 constant-folds every scalar field into immediates
   scattered through the code (equal constants even share one register --
   e.g. inning 3 / score_home 3 / balls 3 all became a single `li`), and the
   Client's u8-patching of the payload could never change them. With the
   barrier, every field is loaded from the payload bytes at runtime. */
#define OPAQUE_PTR(p) __asm__("" : "+r"(p))

/* ---- symbols the setup chain touches that lack a ready header name ------ */
// menu->game rel swap request (a field of the loading-process descriptor at
// 0x80111300; see instant-randoms-boot / boot-directly-to-game memory)
#define trigger_rel_change VAR_ADDRESS(short, 0x80111310)
// menu-side "second human exists" copy (game-rel copy is
// Static_Stats_Tables.player2Ind_; the headers only label the address)
#define player2Ind2_ VAR_ADDRESS(u8, player2Ind2__ADDR)
// claimed free memory: post-start outs-burst frame counter (see phase 2).
// Shares Boot Directly To Game's u8 -- fine, the two never run together.
#define tbm_outsCounter VAR_ADDRESS(u8, 0x802EC01A)

// Runner-on-base staging (1B/2B/3B), addresses + instructions from the proven
// menu-walk generator. The clear instruction stores the runner's roster id to
// 0 during game init; nop'ing it lets a staged runner survive, then it is
// restored so normal play resumes.
#define tbm_runnerRosterLoc(i)  VAR_ADDRESS(u16,  0x8088F04C + (i) * 0x154)
#define tbm_runnerCharID(i)     VAR_ADDRESS(u16,  0x8088F04E + (i) * 0x154)
#define tbm_runnerClearInstr(i) VAR_ADDRESS(u32, 0x806C9420 + (i) * 0x30)
// original store instructions the clear-nop replaces (1B differs from 2B/3B).
static inline u32 tbm_runnerRestoreInstr(int i) { return (i == 0) ? 0xB0650234 : 0xB06500E0; }

// Same split-application constants as Boot Directly To Game.c: booting with
// outs already on the board silences the first at-bat's BGM, so the outs are
// applied a few seconds AFTER the match starts, not during the load.
#define TBM_OUTS_DELAY_FRAMES 180
#define TBM_OUTS_BURST_FRAMES 10


/* =========================================================================
   Stage the predetermined roster into the same fields Instant Randoms fills,
   then apply the spec's extra selections (settings/handedness/superstar/logo)
   that Instant Randoms leaves at defaults. Runs once: it flips `inningSetting.rel` to 5 up
   front, so the C0's `inningSetting.rel != 4` guard keeps it from re-entering.

   ORDER is loadDemoMatch's (0x80642054), preserved from Instant Randoms.c. The
   menu-inningSetting.rel calls (copyInfoToInMemRoster & friends, all at 0x8064xxxx) are safe
   because at inningSetting.rel == 4 the menu inningSetting.rel is still resident in RAM -- flipping `inningSetting.rel`
   only REQUESTS the swap; the teardown happens later in the loading process.
   ========================================================================= */
static inline void TestBoot_StageMatch(const BootMatchSpec* s, const BootStateConfig* c)
{
    // Request the menu->game rel swap (the proven force-swap trigger).
    inningSetting.rel = 5;
    trigger_rel_change = 1;
    sndFXStartEx(0x1bb, 0x40, 0x3f, 0x0); // rio bat SFX -- signals the boot

    // Register who is playing -- what capSS load + "P2 press A to join" do.
    if (s->isCpuMatch)
    {
        // UNVERIFIED path. One human on port 1, CPU opponent.
        g_d_GameSettings.p2_CPU_match_code = P2_CPU_CODE_1_PLAYER_GAME;
        Static_Stats_Tables.playerNumberByPort[0] = 0;
        Static_Stats_Tables.portsActiveInMatch[0] = 0;      // 0 = active
        Static_Stats_Tables.portsActiveInMatch[1] = 0xFF;
        Static_Stats_Tables.portsActiveInMatch[2] = 0xFF;
        Static_Stats_Tables.portsActiveInMatch[3] = 0xFF;
        Static_Stats_Tables.player2Ind_ = 0;
        player2Ind2_                    = 0;
    }
    else
    {
        // Two humans -- the tested Instant Randoms configuration.
        g_d_GameSettings.p2_CPU_match_code = P2_CPU_CODE_2_PLAYER_GAME;
        Static_Stats_Tables.playerNumberByPort[0] = 0;      // P1 = port 1
        Static_Stats_Tables.playerNumberByPort[1] = (u8)(s->p2Port - 1);
        Static_Stats_Tables.portsActiveInMatch[0] = 0;
        Static_Stats_Tables.portsActiveInMatch[1] = 0;
        Static_Stats_Tables.portsActiveInMatch[2] = 0xFF;
        Static_Stats_Tables.portsActiveInMatch[3] = 0xFF;
        Static_Stats_Tables.player2Ind_ = 1;
        player2Ind2_                    = 1;
    }

    // Captains (Instant Randoms rolls these; here they are fixed).
    Static_Stats_Tables.captainSelectedID[0] = s->captain[0];
    Static_Stats_Tables.captainSelectedID[1] = s->captain[1];

    // Character-grid bookkeeping the setup chain still expects. Clear the grid,
    // then mark every roster character taken (a fully-drafted grid).
    for (int i = 0; i < 54; i++)
        Static_Stats_Tables.charOnCharacterGridSelected[i] = 0;
    for (int team = 0; team < 2; team++)
        for (int slot = 0; slot < 9; slot++)
            Static_Stats_Tables.charOnCharacterGridSelected[s->roster[team][slot]] = 1;

    // Stage the roster in the game's native DRAFT-slot order (spec.roster is now
    // draft order, matching the HUD's "Roster N" indexing), plus the fielding
    // position each slot plays (positionSwapMapping). Together these are what the
    // conversion chain reads to build the live on-field defense table; feeding
    // them verbatim reproduces the exact fielders. positionSwapMapping MUST be
    // set before the chain, which reads it (characterSelectScreen does; nothing
    // in the chain writes it). The batting order is separate -- it comes from
    // the char hook's inMemRoster and the mapping in ApplyGameState, so it is
    // unaffected by the roster's slot ordering.
    for (int team = 0; team < 2; team++)
    {
        for (int slot = 0; slot < 9; slot++)
        {
            Roster_Player1.rosterCharID[team][slot]        = s->roster[team][slot];
            Roster_Player1.positionSwapMapping[team][slot] = c->positionSwap[team][slot];
            Roster_Player1.rosterSpotFilledInd[team][slot] = 1;
        }
    }

    // Conversion chain, once, in loadDemoMatch's order (unchanged from Instant
    // Randoms). copyInfoToInMemRoster builds inMemRoster from rosterCharID.
    copyInfoToInMemRoster();
    teamLogoDetermination(0);
    teamLogoDetermination(1);
    unsure_FillRosterPositions(0);
    unsure_FillRosterPositions(1);
    characterSelectScreen(0);
    characterSelectScreen(1);
    setCaptainLocInRoster();

    // Fixed stadium instead of selectRandomStadium(): map the spec's cursor
    // index through the game's own cursor->stadium table. StadiumID (0x800E8705)
    // is what selectRandomStadium writes, so we write the same field directly.
    g_d_GameSettings.StadiumID = (EnumStadiumIDs)cursorToStadIDMapping[s->stadiumCursor];

    // Match settings the spec carries. These are the settled option globals in
    // the 0x800E87xx block right next to batsFirstSetting (which Instant Randoms
    // already sets), so they are the natural home for the force-swap flow.
    g_d_GameSettings.home_AwaySetting            = s->firstBatter;   // 0 = P1 away/home
    inningSetting.inningCount               = s->innings;
    inningSetting.starSkillsSetting  = s->starSkills;
    inningSetting.runsNeededForMercy          = s->mercy ? 10 : 0; // MSSB mercy = 10 runs;
                                                     // 0/off handling UNVERIFIED

    // Derive g_d_GameSettings.playerPorts from playerNumberByPort (Instant Randoms).
    setPortOfEachPlayer();

    // Handedness / superstar are applied per NATURAL batting slot in
    // TestBoot_ApplyGameState (rel 5), after the char hook has reordered the
    // in-memory roster -- doing it here in position order would land them on
    // the wrong players once the roster is in batting order.
    Static_Stats_Tables.capLocationInOrder_P1_P2_[0] = s->captainOrderLoc[0];
    Static_Stats_Tables.capLocationInOrder_P1_P2_[1] = s->captainOrderLoc[1];
    Static_Stats_Tables.teamName_P1_P2_[0]           = s->logo[0];
    Static_Stats_Tables.teamName_P1_P2_[1]           = s->logo[1];

    // Reset the post-start outs counter for this boot (phase 2 below).
    tbm_outsCounter = 0;
}

/* =========================================================================
   Land the match on a specific game state. Ported from Boot Directly To
   Game.c's BootToMatch_ApplyGameState -- proven in the menu-walk flow. In this
   force-swap flow the only match reachable is the one we just booted, and
   hasGameStarted_ is 0 ONLY during that load, so the gate is self-limiting: it
   never touches a normally-played match.
   ========================================================================= */
static inline void TestBoot_ApplyGameState(const BootStateConfig* c)
{
    if (!c->apply_state || inningSetting.rel != 5)
        return;

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

        /* ---- batting order, fielding positions, current batter ----
           characterSelectScreen() built a default (position-order) mapping
           during staging; overwrite it with the real order every frame, the
           same way the score/inning fields above are re-applied. slot 0 of the
           per-team mapping is the pitcher copy (batting location, position 0);
           slots 1-9 are the batting order, and because orderPos/orderChar are
           current-batter-first, currentBatter is always slot 1. */
        if (c->apply_order)
        {
            for (int team = 0; team < 2; team++)
            {
                for (int i = 0; i < 9; i++)
                {
                    int pos = c->orderPos_A_H[team][i];
                    // [0] is the batter's index into the (natural-order)
                    // inMemRoster, not a charID. Since the char hook builds
                    // inMemRoster in this same order, batting slot i+1 is
                    // inMemRoster[i], so the index is simply i.
                    g_GameLogic.battingOrderAndPositionMapping[team][i + 1][0] = i;
                    g_GameLogic.battingOrderAndPositionMapping[team][i + 1][1] = pos;
                    if (pos == 0)
                    {
                        // pitcher copy: the pitcher's inMemRoster index, position 0
                        g_GameLogic.battingOrderAndPositionMapping[team][0][0] = i;
                        g_GameLogic.battingOrderAndPositionMapping[team][0][1] = 0;
                    }
                }
                g_GameLogic.currentBatterPerTeam[team] = c->currentBatter_A_H[team];
            }

            // Handedness / superstar per NATURAL batting slot. The char hook
            // built inMemRoster in that order, so slot i holds the character
            // whose handedness is orderFieldHand_P1P2[team][i]. inMemRoster is
            // P1/P2-indexed (team 0 = P1), matching these arrays.
            for (int team = 0; team < 2; team++)
            {
                for (int i = 0; i < 9; i++)
                {
                    inMemRoster[team][i].stats.FieldingArm    = c->orderFieldHand_P1P2[team][i];
                    inMemRoster[team][i].stats.BattingStance  = c->orderBatHand_P1P2[team][i];
                    inMemRoster[team][i].stats.UnusedBytes[0] = c->orderSuperstar_P1P2[team][i];
                }
            }
        }

        // Runners on base. Write each occupied base's runner and nop the
        // instruction that would clear it during init, so it survives to the
        // live match; TestBoot_ApplyGameState restores that instruction once the
        // match has started.
        for (int i = 0; i < 3; i++)
        {
            if (c->runner_present[i])
            {
                tbm_runnerRosterLoc(i)  = c->runner_rosterLoc[i];
                tbm_runnerCharID(i)     = c->runner_charID[i];
                tbm_runnerClearInstr(i) = 0x60000000; // nop
            }
        }
        return;
    }

    /* ---- once the match has started: put the runner-clear instructions back
       so normal base running works. The runner already survived the load via
       the nop above. Runs before the outs-delay bookkeeping below. */
    for (int i = 0; i < 3; i++)
    {
        if (c->runner_present[i])
            tbm_runnerClearInstr(i) = tbm_runnerRestoreInstr(i);
    }

    /* ---- after the match begins: the outs, and only now ----
       Booting with outs already on the board silences the first at-bat's BGM.
       CONFIRMED for THIS force-swap flow too (2026-07-20): applying the outs
       inline during the load killed the first-at-bat music, exactly as in the
       menu-walk flow. So let the match start clean, then put the outs on a few
       seconds later. */
    if (tbm_outsCounter >= TBM_OUTS_DELAY_FRAMES + TBM_OUTS_BURST_FRAMES)
        return;                                  // done; hands off the match
    tbm_outsCounter++;
    if (tbm_outsCounter <= TBM_OUTS_DELAY_FRAMES)
        return;                                  // let the BGM get going first

    // The delay lands this on a live match, so never overwrite the outs once a
    // pitch is actually in flight.
    if (AtBat_PitchThrown != 0)
    {
        tbm_outsCounter = TBM_OUTS_DELAY_FRAMES + TBM_OUTS_BURST_FRAMES;  // give up
        return;
    }

    g_Strikes.outs       = c->outs;
    g_Strikes.storedOuts = c->outs;
}

/* =========================================================================
   PER-FRAME ENTRY. No .address, so cgecko emits this as a C0 that runs every
   frame; no .state either, so it runs in every inningSetting.rel state -- exactly like
   InstantRandoms(). Everything above is its support code.
   ========================================================================= */
CGECKO(TestNewBootMatch);
void TestNewBootMatch()
{
    // Opaque views of the payload: forces every field to be a runtime load
    // from the patchable blob (see BootMatchPayload above).
    const BootMatchSpec*   s = &boot_payload.spec;
    const BootStateConfig* c = &boot_payload.config;
    OPAQUE_PTR(s);
    OPAQUE_PTR(c);

    // At the menu (rel 4): stage the predetermined match and request the swap.
    // Flipping rel to 5 inside TestBoot_StageMatch makes this a one-shot -- the
    // next frame this guard is false. (Returning to the menu re-boots, same as
    // Instant Randoms.)
    if (inningSetting.rel == 4)
        TestBoot_StageMatch(s, c);

    // During the ensuing match load (rel 5, before first pitch): drop the match
    // onto the predetermined game state.
    TestBoot_ApplyGameState(c);
}


// Fix for duplicate characters. No .instruction on either hook: both REPLACE the
// instruction they overwrite rather than re-running it.
CGECKO(DupLoadBindCharID, .address = 0x800156B0);
void DupLoadBindCharID()
{
    READ_GAME_REG(u32, entry, 5);                  // r5 = &table[r9]
    u32 obj = *(u32*)entry;                        // table[r9]
    WRITE_GAME_REG(3, obj ? obj : 0x80370F1C);
}

CGECKO(DupLoadBindStore, .address = 0x800156E4);
void DupLoadBindStore()
{
    // Two GPRs needed, so read the BACKUP frame directly -- READ_GAME_REG
    // declares its own r30 alias and cannot be used twice in one function. Same
    // layout it uses: saved r<n> at r30 + 0x8 + (n-3)*4.
    register u32 _fp __asm__("r30");
    u32 r3    = *(volatile u32*)(_fp + 0x8);              // saved r3 = table[r9]
    u32 r8    = *(volatile u32*)(_fp + 0x8 + ((8 - 3) << 2)); // saved r8
    u32 model = *(u32*)(0x8036E548 + (r8 << 2) + 11456);  // model[r8]
    *(u32*)((r3 ? r3 : 0x80370F1C) + 24) = model;          // bind; scratch on NULL
}
