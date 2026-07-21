/*###########################################################
# Test New Boot Match
###########################################################*/
// Author: LittleCoaks
// *Boots directly to a Predetermined match. Used in Fast Reset.

#include "Include/Menu/MenuData.h"
#include "Include/Match/MatchData.h"

/* =========================================================================
   Caller-supplied description of the match to boot into.
   Copied verbatim from Boot Directly To Game.c so specs are interchangeable.
   Rosters and per-player arrays are in POSITION order:
     0=P 1=C 2=1B 3=2B 4=3B 5=SS 6=LF 7=CF 8=RF
   ========================================================================= */
typedef struct BootMatchSpec
{
    byte captain[2];            // charID of each team's captain
    byte roster[2][9];          // charIDs, position order
    byte battingHand[2][9];     // 0 = right, 1 = left
    byte fieldingHand[2][9];    // 0 = right, 1 = left
    byte superstar[2][9];       // 0 = off, 1 = on
    byte captainOrderLoc[2];    // captain's slot in the batting order, 0-8
    byte logo[2];               // team logo id, 0-0x2F

    byte stadiumCursor;         // stadium-select CURSOR index, not the in-game
                                // stadium id: 0=Mario 1=Bowser 2=Wario
                                // 3=Yoshi 4=Peach 5=DK. Toy Field is NOT on
                                // this table -- a Toy Field boot needs the
                                // selectRandomStadium toy-field patch instead.
    byte firstBatter;           // 0 = P1 bats first, 1 = P2
    byte starSkills;            // 0 = off, 1 = on
    byte innings;               // actual inning count
    byte mercy;                 // 0 = off, 1 = on

    /* ---- who is playing ---- */
    byte isCpuMatch;            // 1 = P1 vs CPU (only port 1 is a human).
                                //     UNVERIFIED in the force-swap flow -- the
                                //     tested path is two humans (isCpuMatch 0).
    byte p2Port;                // physical controller port for the second
                                // human, 2-4 (1-based). Ignored when isCpuMatch.
} BootMatchSpec;

/* =========================================================================
   Where in a game to land once the match starts. Applied every pre-pitch
   frame because game init overwrites these values partway through the load.
   Copied verbatim from Boot Directly To Game.c.
   ========================================================================= */
typedef struct BootStateConfig
{
    byte apply_state;        // 0 = fresh match; 1 = jump to the state below
    byte inning;             // current inning, 1-based. KEEP IT <= the spec's
                             // `innings` (a value past regulation boots into
                             // extras and the loader never stages that BGM).
    byte bottom_of_inning;   // 0 = top, 1 = bottom
    u16  score_away;         // total runs (also written to inning 1's box)
    u16  score_home;
    byte balls;              // 0-3
    byte strikes;            // 0-2
    byte outs;               // 0-2
    byte stars_p1;           // 0-5
    byte stars_p2;
    byte star_chance;        // 0 = off, 1 = star chance active
} BootStateConfig;

/* =========================================================================
   THE MATCH TO BOOT INTO. Everything the boot needs comes from here.
   Rosters are in POSITION order: P, C, 1B, 2B, 3B, SS, LF, CF, RF.
   (Placeholder values -- every field is set away from its default so it is
   individually confirmable on screen, same convention as Boot Directly.)
   ========================================================================= */
static const BootMatchSpec boot_spec =
{
    .captain = { enumCharIDByte_Bowser, enumCharIDByte_Peach },
    .roster = {
        { enumCharIDByte_Bowser, enumCharIDByte_Peach, enumCharIDByte_DryBones_R_, enumCharIDByte_Birdo, enumCharIDByte_Toad_B_, enumCharIDByte_Luigi, enumCharIDByte_Koopa_G_, enumCharIDByte_Paratroopa_R_, enumCharIDByte_Magikoopa_B_ },
        { enumCharIDByte_Peach, enumCharIDByte_Peach, enumCharIDByte_Toadette, enumCharIDByte_Noki_B_, enumCharIDByte_Pianta_R_, enumCharIDByte_Yoshi, enumCharIDByte_Monty, enumCharIDByte_ShyGuy_R_, enumCharIDByte_Boo },
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
};

/* ---- boot-to-state: the exact point in the game to land on ------------- */
static const BootStateConfig boot_config =
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
    .star_chance      = 0,
};

/* ---- symbols the setup chain touches that lack a ready header name ------ */
// menu->game rel swap request (a field of the loading-process descriptor at
// 0x80111300; see instant-randoms-boot / boot-directly-to-game memory)
#define trigger_rel_change VAR_ADDRESS(short, 0x80111310)
// menu-side "second human exists" copy (game-rel copy is
// Static_Stats_Tables.player2Ind_; the headers only label the address)
#define player2Ind2_ VAR_ADDRESS(byte, player2Ind2__ADDR)
// claimed free memory: post-start outs-burst frame counter (see phase 2).
// Shares Boot Directly To Game's byte -- fine, the two never run together.
#define tbm_outsCounter VAR_ADDRESS(byte, 0x802EC01A)

// Same split-application constants as Boot Directly To Game.c: booting with
// outs already on the board silences the first at-bat's BGM, so the outs are
// applied a few seconds AFTER the match starts, not during the load.
#define TBM_OUTS_DELAY_FRAMES 180
#define TBM_OUTS_BURST_FRAMES 10


/* =========================================================================
   Stage the predetermined roster into the same fields Instant Randoms fills,
   then apply the spec's extra selections (settings/handedness/superstar/logo)
   that Instant Randoms leaves at defaults. Runs once: it flips `rel` to 5 up
   front, so the C0's `rel != 4` guard keeps it from re-entering.

   ORDER is loadDemoMatch's (0x80642054), preserved from Instant Randoms.c. The
   menu-rel calls (copyInfoToInMemRoster & friends, all at 0x8064xxxx) are safe
   because at rel == 4 the menu rel is still resident in RAM -- flipping `rel`
   only REQUESTS the swap; the teardown happens later in the loading process.
   ========================================================================= */
static inline void TestBoot_StageMatch(const BootMatchSpec* s)
{
    // Request the menu->game rel swap (the proven force-swap trigger).
    rel = 5;
    trigger_rel_change = 1;
    sndFXStartEx(0x1bb, 0x40, 0x3f, 0x0); // rio bat SFX -- signals the boot

    // Register who is playing -- what capSS load + "P2 press A to join" do.
    if (s->isCpuMatch)
    {
        // UNVERIFIED path. One human on port 1, CPU opponent.
        matchInfo.p2_CPU_match_code = enum_p2_CPU_code__1PlayerGame;
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
        matchInfo.p2_CPU_match_code = enum_p2_CPU_code__2PlayerGame;
        Static_Stats_Tables.playerNumberByPort[0] = 0;      // P1 = port 1
        Static_Stats_Tables.playerNumberByPort[1] = (byte)(s->p2Port - 1);
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

    // Stage the rosters. Position order, all 9 slots written directly -- the
    // same layout copyInfoToInMemRoster reads and the same convention Boot
    // Directly To Game.c uses (rosterCharID[team][slot] = roster[team][slot]),
    // NOT Instant Randoms' "slot 0 = captain, 1..8 random" draft convention.
    for (int team = 0; team < 2; team++)
    {
        for (int slot = 0; slot < 9; slot++)
        {
            Roster_Player1.rosterCharID[team][slot]        = s->roster[team][slot];
            Roster_Player1.rosterSpotFilledInd[team][slot] = 1;
        }
    }

    // Conversion chain, once, in loadDemoMatch's order (unchanged from Instant
    // Randoms). copyInfoToInMemRoster builds inMemRoster from rosterCharID.
    copyInfoToInMemRoster();
    teamLogoDetermination(0);
    teamLogoDetermination(1);
    challengeSetCPURoster2(0);
    challengeSetCPURoster2(1);
    setInitialBattingOrder(0);
    setInitialBattingOrder(1);
    setCaptainLocInRoster_();

    // Fixed stadium instead of selectRandomStadium(): map the spec's cursor
    // index through the game's own cursor->stadium table. StadiumID (0x800E8705)
    // is what selectRandomStadium writes, so we write the same field directly.
    StadiumID = (EnumStadiumIDs)cursorToStadIDMapping[s->stadiumCursor];

    // Match settings the spec carries. These are the settled option globals in
    // the 0x800E87xx block right next to batsFirstSetting (which Instant Randoms
    // already sets), so they are the natural home for the force-swap flow.
    batsFirstSetting            = s->firstBatter;   // 0 = P1 away/home
    inningSetting               = s->innings;
    starSkillsSetting_800e8758  = s->starSkills;
    runsNeededForMercy          = s->mercy ? 10 : 0; // MSSB mercy = 10 runs;
                                                     // 0/off handling UNVERIFIED

    // Derive matchInfo.playerPorts from playerNumberByPort (Instant Randoms).
    setPortOfEachPlayer();

    // ---- spec extras copyInfoToInMemRoster does not set from the roster ----
    // Applied AFTER the chain so they win over character defaults / the logo
    // and batting-order helpers above. Handedness and the superstar flag live
    // in the in-memory roster; note the roster there is in BATTING order, so if
    // the wrong players get these, that ordering mismatch is why (same caveat
    // as Boot Directly To Game.c). UNVERIFIED in the force-swap flow.
    for (int team = 0; team < 2; team++)
    {
        for (int i = 0; i < 9; i++)
        {
            CharacterStats* p = &inMemRoster_P1_P2_[team][i];
            p->Stats.FieldingArm    = s->fieldingHand[team][i];
            p->Stats.BattingStance  = s->battingHand[team][i];
            p->Stats.UnusedBytes[0] = s->superstar[team][i]; // star flag byte;
                                                             // may also need the
                                                             // cursor-walk (see
                                                             // Boot Directly)
        }
    }
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
    if (!c->apply_state || rel != 5)
        return;

    if (gameControls.EventTriggers_GameHasStarted_ == 0)
    {
        /* ---- during the load: everything EXCEPT the outs ---- */
        // Game init overwrites these partway through, so re-apply every frame.
        GameState.Inning     = c->inning;
        GameState.halfInning = c->bottom_of_inning;
        gameControls.battingTeam_A_H_  = c->bottom_of_inning ? 1 : 0;
        gameControls.fieldingTeam_A_H_ = c->bottom_of_inning ? 0 : 1;

        GameState.scores[0].total       = c->score_away;
        GameState.scores[0].byInning[0] = c->score_away;  // book all runs in the 1st
        GameState.scores[1].total       = c->score_home;
        GameState.scores[1].byInning[0] = c->score_home;

        BallsStrikesOutsStruct.Balls   = c->balls;
        BallsStrikesOutsStruct.Strikes = c->strikes;

        gameControls.TeamStars_P1_P2_[0] = c->stars_p1;
        gameControls.TeamStars_P1_P2_[1] = c->stars_p2;
        gameControls.starChanceCd = c->star_chance;
        return;
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

    BallsStrikesOutsStruct.Outs       = c->outs;
    BallsStrikesOutsStruct.StoredOuts = c->outs;
}

/* =========================================================================
   C0 ENTRY. Everything above this line is the C0's preamble; this is the
   leading region before the first '// Address:', so cgecko emits it as a C0
   (runs every frame, in every rel state) -- exactly like InstantRandoms().
   ========================================================================= */
void TestNewBootMatch()
{
    // At the menu (rel 4): stage the predetermined match and request the swap.
    // Flipping rel to 5 inside TestBoot_StageMatch makes this a one-shot -- the
    // next frame this guard is false. (Returning to the menu re-boots, same as
    // Instant Randoms.)
    if (rel == 4)
        TestBoot_StageMatch(&boot_spec);

    // During the ensuing match load (rel 5, before first pitch): drop the match
    // onto the predetermined game state.
    TestBoot_ApplyGameState(&boot_config);
}


// Fix for duplicate characters
// Address: 0x800156B0
void DupLoadBindCharID()
{
    READ_GAME_REG(word, entry, 5);                  // r5 = &table[r9]
    word obj = *(word*)entry;                        // table[r9]
    WRITE_GAME_REG(3, obj ? obj : 0x80370F1C);
}

// Address: 0x800156E4
void DupLoadBindStore()
{
    // Two GPRs needed, so read the BACKUP frame directly -- READ_GAME_REG
    // declares its own r30 alias and cannot be used twice in one function. Same
    // layout it uses: saved r<n> at r30 + 0x8 + (n-3)*4.
    register word _fp __asm__("r30");
    word r3    = *(volatile word*)(_fp + 0x8);              // saved r3 = table[r9]
    word r8    = *(volatile word*)(_fp + 0x8 + ((8 - 3) << 2)); // saved r8
    word model = *(word*)(0x8036E548 + (r8 << 2) + 11456);  // model[r8]
    *(word*)((r3 ? r3 : 0x80370F1C) + 24) = model;          // bind; scratch on NULL
}
