/*###########################################################
# Boot Directly To Game
###########################################################*/
// Author: LittleCoaks

#include "Game Data/MenuData.h"

#define m_rel VAR_ADDRESS(short, 0x800e877c)
#define m_unk_state_var VAR_ADDRESS(short, 0x80111310)

// WIP
// Boots to P1 vs P2, but game crashes when pausing

// State: Menu
void BootDirectlyToGame()
{
    // this stuff is copied from the load game rel code in the game. it probably needs to be here
    m_rel = 5;
    m_unk_state_var = 1;
    sndFXStartEx(enumSoundEffect_short_openZMenu, 0x40, 0x3f, 0x0); // 0x1bb = play rio bat sound effect to show game is starting

    p2_CPU_code_ = enum_p2_CPU_code__2PlayerGame;
    playerPorts[0] = 0; // P1 = port 1
    playerPorts[1] = 1; // P2 = port 2
    prepareToLoadGame_(9, 1, 1); // 9, 0, 1 successfully loads a P1 vs CPU game

    /*
    FUN_80067f70(0);
    FUN_80067f70(1); 
    Static_Stats_Tables.charOnCharacterGridSelected[Static_Stats_Tables.captainSelectedID[0]] = 1;
    Static_Stats_Tables.charOnCharacterGridSelected[Static_Stats_Tables.captainSelectedID[1]] = 1;
    randomCharacters(0); // for the first version of this code, we'll just draft random teams so it's eazy
    randomCharacters(1);

    //FUN_80067f70(0);
    //FUN_80067f70(1);
    copyInfoToInMemRoster();
    teamLogoDetermination(0);
    teamLogoDetermination(1);
    challengeSetCPURoster2(0);
    challengeSetCPURoster2(1);
    setInitialBattingOrder(0);
    setInitialBattingOrder(1);
    setCaptainLocInRoster_();
    */


    /*
    // try to select captains
    teamManagement_cursorPos[0] = 0; // set team 0 cursor to mario
    teamManagement_cursorPos[1] = 1; // set team 1 cursor to luigi
    
    // these two crash the game
    captainSelect_APress(0); // select mario for team 0
    captainSelect_APress(1); // select luigi for team 1

    // add_RemoveCharToATeam(0, 0, 1); // add mario captain? to team 0

    copyInfoToInMemRoster();
    // setCaptainLocInRoster_();
    // characterSelectAPress(0); // select character for team0 based on cursor position*/
}
