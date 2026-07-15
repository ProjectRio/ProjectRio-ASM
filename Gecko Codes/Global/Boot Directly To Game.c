/*###########################################################
# Boot Directly To Game
###########################################################*/
// Author: LittleCoaks

// WIP -- boots straight into a P1 vs P2 exhibition match with preset
// teams on a random stadium.
//
// No loadDemoMatch: its setup is replicated below with generic menu
// helpers, in the same order it runs them (from the menu-rel disassembly
// of loadDemoMatch at 0x80642054):
//   captain grid bookkeeping -> draft -> copyInfoToInMemRoster ->
//   teamLogoDetermination -> challengeSetCPURoster2 ->
//   setInitialBattingOrder -> setCaptainLocInRoster_ -> selectRandomStadium
// The draft itself (0x80067F70 auto-draft from captain preference tables)
// is replaced by writing the 9 character IDs per team directly into the
// draft array copyInfoToInMemRoster reads.
//
// What the game rel needs staged before the rel swap (the load crash
// when forcing the rel raw): populated inMemRoster / logos / batting
// order / captain loc / stadium ID, plus per-player port data
// (playerNumberByPort, portsActiveInMatch, player2Ind_) for a 2-human
// game -- the menus normally set those on captain-select load + P2 join.
//
// DUPLICATE CHARACTERS require the companion code "Allow Duplicate
// Character Load" to be enabled. Why (savestate forensics 2026-07-03,
// see boot-directly-to-game memory): the menu preloads each unique
// character's MODEL into a shared cache (0x80118220) as you draft in
// team-select over many frames; the loading screen then REUSES those
// cached models across duplicate slots. Booting skips team-select, so
// the cache is empty and the loading screen takes its fresh-load path,
// whose bind loop (0x800156B0) derefs the null object-table entries left
// by de-duping -> DSI at 0x800156B8. The companion code null-checks that
// loop so the boot can use duplicate rosters without the preload.

#include "Game Data/MenuData.h"

#define m_rel VAR_ADDRESS(short, 0x800e877c)
#define trigger_rel_change VAR_ADDRESS(short, 0x80111310)

// port of player N (0-based port; set on P2 join in the normal flow)
#define playerNumberByPort_ ARRAY_1D_ADDRESS(byte, 4, 0x80353098)
// per-PORT flags: 0 = active in match, 0xFF = inactive
// (onlySetPort1ToActiveOnInitialCapSSLoad sets {0,-1,-1,-1})
#define portsActiveInMatch_ ARRAY_1D_ADDRESS(byte, 4, 0x8035309C)
// Static_MSSB_Data.player2Ind_ -- game rel's "second human exists" flag
#define player2Ind_static VAR_ADDRESS(byte, 0x803530C9)
// menu-side copy set by checkForNewPlayer when P2 joins
#define player2Ind2_ VAR_ADDRESS(byte, 0x803C5EA9)

// captain char IDs consumed by the setup chain (ints!)
#define captainSelectedID_ ARRAY_1D_ADDRESS(int, 2, 0x80353080)
// 54 per-grid-cell "character taken" flags, indexed by char ID
#define charOnGridSelected_ ARRAY_1D_ADDRESS(byte, 54, 0x803530F7)
// the drafted rosters copyInfoToInMemRoster reads: [team][slot], slot 0 = captain
#define draftedChars ARRAY_2D_ADDRESS(byte, 2, 9, 0x803C6726)
#define home_awaySetting_ VAR_ADDRESS(byte, 0x800E870A)

#define CHAR_DK    0x2
#define CHAR_YOSHI 0x6

// preset teams: all Yoshi vs all DK (dupes need "Allow Duplicate Character Load")
static const byte team0_chars[9] = { CHAR_YOSHI, CHAR_YOSHI, CHAR_YOSHI, CHAR_YOSHI, CHAR_YOSHI, CHAR_YOSHI, CHAR_YOSHI, CHAR_YOSHI, CHAR_YOSHI };
static const byte team1_chars[9] = { CHAR_DK, CHAR_DK, CHAR_DK, CHAR_DK, CHAR_DK, CHAR_DK, CHAR_DK, CHAR_DK, CHAR_DK };

// State: Menu
void BootDirectlyToGame()
{
    // this stuff is copied from the load game rel code in the game. it probably needs to be here
    m_rel = 5;
    trigger_rel_change = 1;
    sndFXStartEx(enumSoundEffect_short_openZMenu, 0x40, 0x3f, 0x0); // 0x1bb = play rio bat sound effect to show game is starting

    // register both humans -- what capSS load + "P2 press A to join" would do
    matchInfo.p2_CPU_match_code = enum_p2_CPU_code__2PlayerGame;
    playerNumberByPort_[0] = 0;      // P1 = port 1
    playerNumberByPort_[1] = 1;      // P2 = port 2
    portsActiveInMatch_[0] = 0;
    portsActiveInMatch_[1] = 0;
    portsActiveInMatch_[2] = 0xFF;
    portsActiveInMatch_[3] = 0xFF;
    player2Ind_static = 1;
    player2Ind2_      = 1;

    // captains + character grid bookkeeping (loadDemoMatch does the same)
    captainSelectedID_[0] = CHAR_YOSHI;
    captainSelectedID_[1] = CHAR_DK;
    for (int i = 0; i < 54; i++)
        charOnGridSelected_[i] = 0;
    charOnGridSelected_[CHAR_YOSHI] = 1;
    charOnGridSelected_[CHAR_DK]    = 1;

    // draft the preset teams (replaces the 0x80067F70 auto-draft)
    for (int slot = 0; slot < 9; slot++)
    {
        draftedChars[0][slot] = team0_chars[slot];
        draftedChars[1][slot] = team1_chars[slot];
    }

    // conversion chain, once, in loadDemoMatch's order
    copyInfoToInMemRoster();
    teamLogoDetermination(0);
    teamLogoDetermination(1);
    FUNCTION_ADDRESS(void, 0x80064A04, int)(0);  // challengeSetCPURoster2(team)
    FUNCTION_ADDRESS(void, 0x80064A04, int)(1);
    setInitialBattingOrder(0);
    setInitialBattingOrder(1);
    setCaptainLocInRoster_();

    selectRandomStadium();
    home_awaySetting_ = 0; // 0 = P1 away / P2 home

    // derive matchInfo.playerPorts (0x800E874C/D) from playerNumberByPort;
    // with p2_CPU_match_code != 0 it takes P2's port from playerNumberByPort[1]
    setPortOfEachPlayer();
}
