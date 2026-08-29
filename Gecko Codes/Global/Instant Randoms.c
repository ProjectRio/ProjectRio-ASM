/*###########################################################
# Boot Directly To Game
###########################################################*/
// Author: LittleCoaks
// *Boots directly to a random 5-inning match. P1 vs P2. Duplicates enabled.
// *Randomizes home/away, all-or-nothing superstars, and star skills.
#include "Include/Local/Legacy.h"


#include "Include/Local/LegacyMenus.h"
// TODO(decomp-migration): Menu data came from the Ghidra-exported MenuData.h.
// Replace with the decomp headers under Include/game/ and remap the
// symbol names -- see Include/_sync_report.txt and the decomp's
// docs/ghidra_name_conflicts.md.

// Everything the setup chain touches has a header name and is used directly:
//   rel (0x800E877C), batsFirstSetting (0x800E870A), and the Static_MSSB_Data
//   fields via Static_Stats_Tables (0x8034E9A0): playerNumberByPort,
//   portsActiveInMatch, player2Ind_, captainSelectedID (ints),
//   charOnCharacterGridSelected. The drafted rosters copyInfoToInMemRoster
//   reads live in Roster_Player1 (structCharSelect @ 0x803C6726).
// Only these two lack a ready header symbol:
#define trigger_rel_change VAR_ADDRESS(short, 0x80111310)   // not labelled in the headers
// menu-side "second human exists" copy, set by checkForNewPlayer on P2 join
// (GlobalData.h gives only the address label; the game-rel copy is
// Static_Stats_Tables.player2Ind_)
#define player2Ind2_ VAR_ADDRESS(u8, player2Ind2__ADDR)

#define N_CAPTAINS 12   // mapCaptainCursorPositionToCharID is the capSS grid


#define N_DRAFTABLE 54   // character IDs 0..53; every slot may repeat any of them

static void DraftRandomTeamWithDupes(int team)
{
    // Roster_Player1 is the structCharSelect at 0x803C6726 that
    // copyInfoToInMemRoster reads: rosterCharID[team][slot], slot 0 = captain.
    // randRange_FUN_80042bf0(high, low) is the header's signature.
    Roster_Player1.rosterCharID[team][0]        = (u8)Static_Stats_Tables.captainSelectedID[team];
    Roster_Player1.rosterSpotFilledInd[team][0] = 1;
    for (int slot = 1; slot < 9; slot++)
    {
        Roster_Player1.rosterCharID[team][slot]        = (u8)randRange_FUN_80042bf0(N_DRAFTABLE - 1, 0);
        Roster_Player1.rosterSpotFilledInd[team][slot] = 1;
    }
}

/* NO .state on this code on purpose: a menu gate here would also have to be a
   menu gate on the whole boot, and this code is what REQUESTS the swap to inningSetting.rel 5
   (it keeps running through the transition). The menu gate is therefore done in
   C, below, and it is not optional: almost everything after it is a menu-inningSetting.rel
   call (copyInfoToInMemRoster and friends live at 0x8064xxxx, which is a
   different inningSetting.rel's code at inningSetting.rel 5). The other codes in this file carry their own
   .state where they need one. */
CGECKO(InstantRandoms);
void InstantRandoms()
{
    if (inningSetting.rel != 4)
        return;

    // this stuff is copied from the load game rel code in the game. it probably needs to be here
    inningSetting.rel = 5;
    trigger_rel_change = 1;
    sndFXStartEx(0x1bb, 0x40, 0x3f, 0x0); // 0x1bb = play rio bat sound effect to show game is starting

    // register both humans -- what capSS load + "P2 press A to join" would do
    g_d_GameSettings.p2_CPU_match_code = P2_CPU_CODE_2_PLAYER_GAME;
    Static_Stats_Tables.playerNumberByPort[0] = 0;      // P1 = port 1
    Static_Stats_Tables.playerNumberByPort[1] = 1;      // P2 = port 2
    Static_Stats_Tables.portsActiveInMatch[0] = 0;      // 0 = active, 0xFF = inactive
    Static_Stats_Tables.portsActiveInMatch[1] = 0;
    Static_Stats_Tables.portsActiveInMatch[2] = 0xFF;
    Static_Stats_Tables.portsActiveInMatch[3] = 0xFF;
    Static_Stats_Tables.player2Ind_ = 1;
    player2Ind2_                    = 1;

    // captains + character grid bookkeeping (loadDemoMatch does the same).
    // Two random captains, drawn with the game's RNG over its own captain-select
    // grid table (Mario, Luigi, Peach, Daisy, Yoshi, Birdo, Wario, Waluigi, DK,
    // Diddy, Bowser, Bowser Jr.) rather than over all 54 character IDs.
    int cap_cell_0 = randRange_FUN_80042bf0(N_CAPTAINS - 1, 0);
    int cap_cell_1 = randRange_FUN_80042bf0(N_CAPTAINS - 1, 0);
    // The real screen stops P2 taking P1's captain. Nudge to the next grid cell
    // instead of re-rolling: a reroll loop could in principle never terminate,
    // and hanging is much worse here than a hair of bias.
    if (cap_cell_1 == cap_cell_0)
        cap_cell_1 = (cap_cell_1 + 1 < N_CAPTAINS) ? cap_cell_1 + 1 : 0;

    u8 captain0 = mapCaptainCursorPositionToCharID[cap_cell_0];
    u8 captain1 = mapCaptainCursorPositionToCharID[cap_cell_1];

    Static_Stats_Tables.captainSelectedID[0] = captain0;
    Static_Stats_Tables.captainSelectedID[1] = captain1;
    // Grid bookkeeping the rest of the setup chain still expects (captains
    // marked, everyone else clear). DraftRandomTeamWithDupes no longer consults
    // this -- it allows duplicates and picks over the whole range -- but
    // copyInfoToInMemRoster and the logo/roster helpers below read it.
    for (int i = 0; i < 54; i++)
        Static_Stats_Tables.charOnCharacterGridSelected[i] = 0;
    Static_Stats_Tables.charOnCharacterGridSelected[captain0] = 1;
    Static_Stats_Tables.charOnCharacterGridSelected[captain1] = 1;

    // random teams, duplicates allowed (reads captainSelectedID for slot 0)
    DraftRandomTeamWithDupes(0);
    DraftRandomTeamWithDupes(1);

    // conversion chain, once, in loadDemoMatch's order
    copyInfoToInMemRoster();
    teamLogoDetermination(0);
    teamLogoDetermination(1);
    unsure_FillRosterPositions(0);
    unsure_FillRosterPositions(1);
    characterSelectScreen(0);
    characterSelectScreen(1);
    setCaptainLocInRoster();

    PatchInstruction_Conditional(0x80067220, 0x38800005, 0x38800006); // allow random toy field
    selectRandomStadium();

    // ---- match settings ----------------------------------------------------
    // Always a 3-inning game. inningSetting (0x800E8754) holds the actual inning
    // COUNT, not a menu cursor index -- Boot To Match.c writes it the same way.
    inningSetting.inningCount = 5;

    // Randomize which of P1/P2 bats first (i.e. who is home vs away).
    // 0 = P1 away / P2 home, 1 = the reverse.
    g_d_GameSettings.home_AwaySetting = (u8)randRange_FUN_80042bf0(1, 0);

    // Superstars are all-or-nothing this match: either every character is a
    // superstar or none are (coin flip).
    int allSuperstars = randRange_FUN_80042bf0(1, 0);

    // Star skills (the match-wide star-swing/pitch toggle, 1 = on, 0 = off):
    //   - superstars ON  -> randomize whether star skills are allowed
    //   - superstars OFF -> star skills are always on
    inningSetting.starSkillsSetting = (u8)(allSuperstars ? randRange_FUN_80042bf0(1, 0) : 1);

    // Apply the superstars. The game's team-management menu stars a character by
    // parking teamManagement_cursorPos on its slot and calling
    // transferStatsToInMemRoster(team) (which reads that cursor, looks the slot's
    // char up in the in-mem roster, and toggles its superstar state + stats).
    // The force-swap boot never visits that menu, so we drive the same primitive
    // directly here -- after the roster chain above, so nothing overwrites it.
    // The cursor is 1-indexed (slot 0 in the roster is cursor value 1).
    if (allSuperstars)
    {
        for (int team = 0; team < 2; team++)
        {
            for (int slot = 1; slot <= 9; slot++)
            {
                teamManagement_cursorPos[team] = (u8)slot;
                transferStatsToInMemRoster(team);
            }
            teamManagement_cursorPos[team] = 0; // park the cursor, as the menu does
        }
    }

    // derive g_d_GameSettings.playerPorts (0x800E874C/D) from playerNumberByPort;
    // with p2_CPU_match_code != 0 it takes P2's port from playerNumberByPort[1]
    setPortOfEachPlayer();
}

CGECKO(ToyFieldBanBunting, .address = 0x80009404, .state = MSSB_GAME,
                           .instruction = "mr r3, r25");
void ToyFieldBanBunting()
{
    if (g_d_GameSettings.StadiumID != STADIUM_ID_TOY_FIELD)
        return;
    PatchInstruction(0x806532F0, 0x48000034);   // b +0x34 -- branch over the bunt path
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
