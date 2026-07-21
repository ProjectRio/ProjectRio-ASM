/*###########################################################
# Boot Directly To Game
###########################################################*/
// Author: LittleCoaks
// *Boots directly to a random match. P1 vs P2. Duplicates enabled.


#include "Include/Menu/MenuData.h"

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
#define player2Ind2_ VAR_ADDRESS(byte, player2Ind2__ADDR)

#define N_CAPTAINS 12   // mapCaptainCursorPositionToCharID is the capSS grid


#define N_DRAFTABLE 54   // character IDs 0..53; every slot may repeat any of them

static void DraftRandomTeamWithDupes(int team)
{
    // Roster_Player1 is the structCharSelect at 0x803C6726 that
    // copyInfoToInMemRoster reads: rosterCharID[team][slot], slot 0 = captain.
    // randRangeInclusive(high, low) is the header's signature.
    Roster_Player1.rosterCharID[team][0]        = (byte)Static_Stats_Tables.captainSelectedID[team];
    Roster_Player1.rosterSpotFilledInd[team][0] = 1;
    for (int slot = 1; slot < 9; slot++)
    {
        Roster_Player1.rosterCharID[team][slot]        = (byte)randRangeInclusive(N_DRAFTABLE - 1, 0);
        Roster_Player1.rosterSpotFilledInd[team][slot] = 1;
    }
}

/* NO file-level `State:` on purpose. It emits a conditional that wraps EVERY
   section, so a file-level menu gate would also gate the game-rel section at
   the bottom and that section would never run. The menu gate is therefore done
   in C, here, and it is not optional: almost everything below is a menu-rel
   call (copyInfoToInMemRoster and friends live at 0x8064xxxx, which is a
   different rel's code at rel 5). */
void InstantRandoms()
{
    if (rel != 4)
        return;

    // this stuff is copied from the load game rel code in the game. it probably needs to be here
    rel = 5;
    trigger_rel_change = 1;
    sndFXStartEx(0x1bb, 0x40, 0x3f, 0x0); // 0x1bb = play rio bat sound effect to show game is starting

    // register both humans -- what capSS load + "P2 press A to join" would do
    matchInfo.p2_CPU_match_code = enum_p2_CPU_code__2PlayerGame;
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
    int cap_cell_0 = randRangeInclusive(N_CAPTAINS - 1, 0);
    int cap_cell_1 = randRangeInclusive(N_CAPTAINS - 1, 0);
    // The real screen stops P2 taking P1's captain. Nudge to the next grid cell
    // instead of re-rolling: a reroll loop could in principle never terminate,
    // and hanging is much worse here than a hair of bias.
    if (cap_cell_1 == cap_cell_0)
        cap_cell_1 = (cap_cell_1 + 1 < N_CAPTAINS) ? cap_cell_1 + 1 : 0;

    byte captain0 = mapCaptainCursorPositionToCharID[cap_cell_0];
    byte captain1 = mapCaptainCursorPositionToCharID[cap_cell_1];

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
    challengeSetCPURoster2(0);
    challengeSetCPURoster2(1);
    setInitialBattingOrder(0);
    setInitialBattingOrder(1);
    setCaptainLocInRoster_();

    PatchInstruction_Conditional(0x80067220, 0x38800005, 0x38800006); // allow random toy field
    selectRandomStadium();
    batsFirstSetting = 0; // 0 = P1 away / P2 home

    // derive matchInfo.playerPorts (0x800E874C/D) from playerNumberByPort;
    // with p2_CPU_match_code != 0 it takes P2's port from playerNumberByPort[1]
    setPortOfEachPlayer();
}

// Address: 0x80009404
// Instruction: mr r3, r25
// State: Game
void ToyFieldBanBunting()
{
    if (StadiumID != EnumStadiumIDs_ToyField)
        return;
    PatchInstruction(0x806532F0, 0x48000034);   // b +0x34 -- branch over the bunt path
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
