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

/*###########################################################################
# Reference: MSB_GenerateQuickMatchSetupGeckoCode.cpp (ProjectRio Dolphin)
###########################################################################
Documentation of every gecko code produced by
Source/Core/Core/MSB_GenerateQuickMatchSetupGeckoCode.cpp (+ .h constants).
That code takes a HUD-file game state (MSBQuickMatchGameState) and emits one
gecko code named "Custom Match State" that force-menus the game into a match
and then patches the live game state. Goal: replicate this behavior natively
in this file.

Gecko codetype legend (base address 0x80000000):
  00 XXXXXX YYYY00ZZ  8-bit  write: byte ZZ written (YYYY+1) times, stride 1
  02 XXXXXX YYYYZZZZ  16-bit write: halfword ZZZZ written (YYYY+1) times
  04 XXXXXX VVVVVVVV  32-bit write
  08 XXXXXX VVVVVVVV  serial write (2 lines); 2nd line TNNNZZZZ WWWWWWWW =
                      size T, NNN+1 writes, addr stride ZZZZ, value step W
  28 XXXXXX MMMMVVVV  16-bit if: (halfword @ addr & ~MMMM) == VVVV
  C2 XXXXXX NNNNNNNN  ASM injection: NN lines of PPC replace instr @ addr
                      (branch out/back)
  E0000000 80008000   FULL terminator: ends ALL open conditionals

===========================================================================
OVERALL STRUCTURE
===========================================================================
28 0E877C 00000004            ; if rel == 4 (main menu)   [0x800E877C, same
                              ;   halfword as m_rel / teamSelectProcess]
    ... SECTION A: menu codes ...
E0000000 80008000
28 0E877C 00000005            ; if rel == 5 (in-game)
28 892AB4 FF000000            ; and byte 0x80892AB5 == 0 (hasGameStarted_)
    ... SECTION B: game-state codes (before first pitch) ...
E0000000 80008000
28 892AB4 FF000001            ; if hasGameStarted_ == 1
    ... SECTION C: restore original instructions ...
E0000000 80008000
E0000000 80008000

NOTE: E0 is a *full* terminator, so the intended nesting
(rel==5 AND started==1) is not actually enforced for Section C — after the
first inner E0 the rel check is cleared, and Section C is gated only on
hasGameStarted_ == 1. Works in practice because that byte is only 1 in-game.

===========================================================================
SECTION A — MAIN MENU (rel == 4). Automates the menus.
===========================================================================
Captain select screen:
  04 353080 <charID>          ; P1 captain char ID   (Team_TeamCaptain_ADDR)
  04 353084 <charID>          ; P2 captain char ID   (valid IDs 0x00-0x35)
  04 6548DC 48000234          ; if both captains given: patch instr at
                              ; 0x806548DC (orig: addi r7,r7,0x154) with
                              ; b +0x234 so P1 spamming A can't grab the CPU
                              ; captain (avoids invalid read)

Team logos (written to the away/home slots, NOT P1/P2 — game hardcodes
"away logo = first batter's logo", so P1/P2 are swapped when halfInning==1):
  00 3530AD 000000<id>        ; away-slot logo (teamNames_ADDR), ids 0-0x2F
  00 3530AE 000000<id>        ; home-slot logo

Character select screen, per team (roster given in position order
P,C,1B,2B,3B,SS,LF,CF,RF; charSelectStruct_803c6726):
  00 3C676E 00080001          ; P1: mark all 9 roster spots filled
                              ;   (rosterSpotFilledInd, 9x 8-bit fill)
  00 3C6777 00080001          ; P2 spot-filled indicators
  00 750C7F 00000001          ; P1 OK button selectable (okButtonSelectable[0])
  00 750C80 00000001          ; P2 OK button selectable
  04 750C48 00000009          ; P1 cursor -> OK button (positionCursorPosition[0]=9)
  04 750C4C 00000009          ; P2 cursor -> OK button
  00 3C6726+i 000000<charID>  ; P1 roster: char ID per position i (0-8), stride 1
  00 3C672F+i 000000<charID>  ; P2 roster: same, base 0x803C672F
  04 64DF60 60000000          ; if both rosters given: nop the cursor-movement
                              ;   call at 0x8064DF60

Batting order screen — C2 injection @ 0x80066A48 (22 lines).
Overwrites a function in the order-setup path. At the hook, r8 = team
(0=P1,1=P2) and r1+0x38 = stack buffer receiving the batting order:
  C2066A48 00000016
  3AE10038 2C080001           ; addi r23,r1,0x38 ; cmpwi r8,1
  41820058 60000000           ; beq -> P2 block ; nop
  9 lines P1:  398000<charID> 999700<slot>   ; li r12,charID ; stb r12,slot(r23)
  48000050 60000000           ; b -> end ; nop
  9 lines P2:  398000<charID> 999700<slot>   ; same pattern
  60000000 00000000           ; terminator
charID per slot = charactersPxByPosition[positionByBattingOrder[slot]].
p1IsAway = (halfInning==0) ? (firstBatter==0) : (firstBatter==1); it picks
whether P1 gets the away or home batting order.

Captain's spot in the batting order (capLocInRoster_ADDR):
  00 3530A9 000000<slot>      ; P1 captain's batting-order slot (0-8)
  00 3530AA 000000<slot>      ; P2

Handedness — C2 injection @ 0x80047E2C (56 lines). Orig instr: lis r4,0x8033.
Walks the in-memory roster struct (0xA0 per player, ordered team-then-
batting-order) writing fielding hand at +0x00 and batting hand at +0x01 of
the handedness field, first entry at 0x80353C06 (0=right, 1=left):
  C2047E2C 00000038
  3C808035 38843C06           ; r4 = 0x80353C06 (team0, roster spot 0)
  18x (both teams, batting-order sequence):
    386000<f> 38C000<b>       ; li r3,fieldingHand ; li r6,battingHand
    98640000 98C40001         ; stb r3,0(r4) ; stb r6,1(r4)
    388400A0 60000000         ; addi r4,r4,0xA0 (next roster spot) ; nop
  3C808033 00000000           ; restore original instruction

Superstars — two parts:
 1) Superstar bools written into the unused byte of each roster-struct entry
    (P1 base 0x80353BE5, P2 base 0x80354185, stride 0xA0, indexed by
    batting-order roster spot):
      if all 9 starred:  08 353BE5 00000001 / 000800A0 00000000  (serial)
      else per player:   00 <base + spot*0xA0> 00000001
 2) C2 injection @ 0x8005A4F4 (20 lines) — this is exactly
    "Gecko Codes/Menu/Auto-Superstar.asm" in this repo (orig instr:
    lis r3,0x8033). Summary: r27 = team; keeps a 1-indexed progress counter
    per team at 0x802EBF99/0x802EBF9A; each pass sets
    teamManagement_cursorPos[team] (0x80336726+team) to the counter, reads
    the superstar bool for that roster spot and stores it into
    InProgress_superStarAPlayer[team] (0x8033677E+team) which makes the game
    superstar the character under the cursor; counter==0xA resets cursor to
    0, >0xA does nothing. Raw lines:
      C205A4F4 00000014
      2C1B0002 41810090 / 3C60802F 3863BF99 / 7C63DA14 8B030000
      2C180000 41820060 / 3FC08033 3BDE6726 / 7FDEDA14 2C18000A
      41800018 41820008 / 4800005C 3B400000 / 9B5E0000 48000038
      9B1E0000 3C608035 / 38633BE5 1FDB0009 / 7FDEC214 3BDEFFFF
      1FDE00A0 7C63F214 / 3FC08033 3BDE677E / 7FDEDA14 8B830000
      9B9E0000 3C60802F / 3863BF99 7C63DA14 / 3B180001 9B030000
      48000004 3C608033 / 60000000 00000000

Lock team-management cursor (only if roster+order+hands+stars all provided):
  04 0463C8 48000074          ; up-press instr @ 0x800463C8:
                              ;   beq +0x74 -> b +0x74 (always skip)
  04 046440 48000074          ; down-press instr @ 0x80046440, same patch

Stadium select screen:
  00 750C37 000000<n>         ; stadium CURSOR position (cursorPos_stadSelect
                              ;   region); 0=Mario 1=Bowser 2=Wario 3=Yoshi
                              ;   4=Peach 5=DK (Toy Field unsupported)
  02 650586 00000000          ; zero the 16-bit immediate of the cursor-right
  02 650536 00000000          ;   and cursor-left instrs (locks cursor)

Game settings screen (values are the menu cursor indices):
  00 3C5F40 000000<v>         ; first batter, 0=P1 1=P2 (firstBattingTeam_ADDR)
  00 3C5F41 000000<v>         ; star skills, 0=off 1=on (starSkills_ADDR)
  00 3C5F42 000000<v>         ; innings: v = (innings-1)>>1, i.e. cursor
                              ;   index, odd innings only (inningSelectionOnMenu)
  00 3C5F43 000000<v>         ; mercy, 0=off 1=on (mercyRule_ADDR)
  02 049616 00000000          ; if all four given: zero immediates of the
  02 0495DA 00000000          ;   settings-screen cursor right/left instrs

===========================================================================
SECTION B — IN-GAME, BEFORE FIRST PITCH (rel==5 && hasGameStarted_==0).
Patches live game state (GameState struct @ 0x808928A0 in GameData.h).
Team-indexed values here are away/home, not P1/P2.
===========================================================================
  04 8928A0 <inning>          ; inning (1-18)
  00 89294D 000000<v>         ; half inning, 0=top 1=bottom
                              ;   (GameControls_bottomOfInningInd_ADDR)
  04 892998 <0|1>             ; batting team (homeTeamBattingInd_ADDR)
  04 89299C <0|1>             ; fielding team (awayTeamBattingInd_ADDR)

Scores (16-bit). If total+innings given and they disagree, total forced to
99 as an error flag; if only total given, it is also written to inning 1;
if only innings given, total = their sum:
  02 8928A4 0000<away total>  ; GameScores_ADDR
  02 8928A6+2i 0000<runs>     ; away per-inning, i=0..17
  02 8928CA 0000<home total>  ; GameControls_Score_Home_ADDR
  02 8928CC+2i 0000<runs>     ; home per-inning

Count (BallsStrikesOutsStruct @ 0x80892968):
  04 892968 <strikes>         ; 0-2
  04 89296C <balls>           ; 0-3
  04 892970 <outs>            ; 0-2 (GameControls_Outs_ADDR)
  04 892974 <outs>            ; stored outs — must match (GameControls_StoredOuts)

Stars:
  00 892AD6 000000<v>         ; P1 team stars, 0-5 (GameControls_Stars_ADDR)
  00 892AD7 000000<v>         ; P2 team stars
  00 892AD8 000000<v>         ; star chance active, 0/1

Batting order / position struct (away base 0x808929C8 = rosterIndex_ADDR,
home base 0x80892A18; 8 bytes per entry: word0 = batting-order slot,
word1 = position; entry 0 duplicates the pitcher, entries 1-9 = order slots):
  04 <base + 8*(slot+1) + 4> <position>   ; position (0-8) for each slot
  and when position == 0 (the pitcher):
  04 <base> <slot>            ; pitcher's slot in the order
  04 <base + 4> 00000000      ; pitcher's position (0)

Runners on base (i = 0,1,2; requires matching roster info; runner struct
stride 0x154):
  02 8F04C+0x154*i 0000<rosterSpot>   ; runner roster spot (0-8)
  02 8F04E+0x154*i 0000<charID>       ; runner char ID (0-0x35)
  04 <0x806C9420 + 0x30*i> 60000000   ; nop the instr that clears the runner
                                      ;   roster ID at game start

Pitcher stamina (16-bit, P1/P2 basis, indexed by batting-order slot,
stride 0x1E; P1 base = Pitcher_Stamina_ADDR):
  02 3535D8+0x1E*i 0000<stamina>      ; P1
  02 3536E6+0x1E*i 0000<stamina>      ; P2

===========================================================================
SECTION C — AFTER GAME START (hasGameStarted_ == 1).
Restores every instruction patched above so gameplay is vanilla.
===========================================================================
  04 <0x806C9420+0x30*i> <orig>       ; un-nop runner-clear instrs; originals
                                      ;   {0xB0650234, 0xB06500E0, 0xB06500E0}
                                      ;   (sth r3,0x234(r5) / sth r3,0xE0(r5))
  04 05A4F4 3C608033          ; restore superstar C2 hook site (lis r3,0x8033)
  04 0463C8 41820074          ; restore team-mgmt up-press (beq +0x74)
  04 046440 41820074          ; restore team-mgmt down-press (beq +0x74)
  04 6548DC 38E70154          ; restore captain-screen instr (addi r7,r7,0x154)

===========================================================================
ADDRESS QUICK REFERENCE (constants from MSB_GenerateQuickMatchSetupGeckoCode.h)
===========================================================================
REL_ADDR                          0x800E877C  (m_rel; 4=menu, 5=in-game)
HAS_GAME_STARTED byte             0x80892AB5  (checked via halfword @ ...AB4)
CAPTAIN_CHARACTER P1/P2           0x80353080 / 0x80353084
CAPTAIN_BATTING_ORDER_LOC P1/P2   0x803530A9 / 0x803530AA
LOGO away/home slots              0x803530AD / 0x803530AE
CAPTAIN_SCREEN_PREVENT_CPU        0x806548DC  (new 0x48000234, orig 0x38E70154)
CHARACTERS base P1/P2 (stride 1)  0x803C6726 / 0x803C672F
SPOT_FILLED P1/P2                 0x803C676E / 0x803C6777
OK_ACTIVE P1/P2                   0x80750C7F / 0x80750C80
CHAR_SELECT_CURSOR P1/P2          0x80750C48 / 0x80750C4C  (OK = 9)
CHAR_SELECT cursor-move call      0x8064DF60  (nop'd)
BATTING ORDER C2 hook             0x80066A48
HANDEDNESS C2 hook                0x80047E2C  (data base 0x80353C06, +0xA0)
SUPERSTAR C2 hook                 0x8005A4F4  (orig 0x3C608033)
SUPERSTAR bools P1/P2 (+0xA0)     0x80353BE5 / 0x80354185
SUPERSTAR progress idx P1/P2      0x802EBF99 / 0x802EBF9A
teamManagement_cursorPos          0x80336726  (+1 for P2)
InProgress_superStarAPlayer       0x8033677E  (+1 for P2)
TEAM_MGMT up/down press instrs    0x800463C8 / 0x80046440  (orig 0x41820074)
STADIUM cursor                    0x80750C37
STADIUM cursor R/L immediates     0x80650586 / 0x80650536
FIRST_BATTER/STARS/INNINGS/MERCY  0x803C5F40 / 41 / 42 / 43
SETTINGS cursor R/L immediates    0x80049616 / 0x800495DA
INNING / HALF_INNING              0x808928A0 / 0x8089294D
BATTING_TEAM / FIELDING_TEAM      0x80892998 / 0x8089299C
SCORE away/home totals            0x808928A4 / 0x808928CA
SCORE per-inning away/home (+2)   0x808928A6 / 0x808928CC
STRIKES/BALLS/OUTS/OUTS_STORED    0x80892968 / 6C / 70 / 74
TEAM_STARS P1/P2, STAR_CHANCE     0x80892AD6 / AD7 / AD8
ORDER+POS struct away/home        0x808929C8 / 0x80892A18  (entry stride 8)
RUNNER rosterSpot/charID (+0x154) 0x8088F04C / 0x8088F04E
RUNNER clear instrs (+0x30)       0x806C9420
PITCHER_STAMINA P1/P2 (+0x1E)     0x803535D8 / 0x803536E6
NOP_INSTR                         0x60000000
###########################################################################*/
