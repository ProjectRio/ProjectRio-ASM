/*###########################################################
# Legacy.h -- Ghidra-era names the decomp has not replaced yet
###########################################################*/
// Hand-written. SyncFromDecomp.py owns Include/game, Include/static and
// Include/Symbols; it never touches Include/Local.
//
// The mods here were written against the Ghidra export (GlobalData.h,
// MatchData.h, MenuData.h). Those headers are gone, replaced by decomp-derived
// ones, and most of what they gave now comes from the decomp. Three things do
// not, and they live here:
//
//   1. Types the decomp does not model yet -- ScreenText, Static_MSSB_Data,
//      controllerInputStruct and friends. Recovered verbatim from the export.
//   2. Objects the decomp names and addresses but does not type. The address
//      comes from Include/Symbols/, the type from (1).
//   3. Fields the old export flattened into their own absolute address. The
//      decomp models these as struct members, so the flattened form is kept
//      here with its derivation recorded.
//
// Every address is anchored on memory rather than on a name: docs/rename_map.md
// derived them by matching the old header's recorded address against the
// decomp's symbol table, so a renamed symbol is still the same bytes. As the
// decomp labels more of the game, entries here should be deleted in favour of
// the real declaration -- this file is meant to shrink.
//
// DOL scope only. The match and menu RELs share one arena slot, so their
// bindings stay in Include/Symbols/game.h and Include/Symbols/menus.h and a mod
// includes whichever one it belongs to.
#pragma once

#include "CGecko/Common.h"
#include "Include/mssbTypes.h"
#include "Include/types.h"
#include "Include/Dolphin/pad.h"
#include "Include/static/UnknownHomes_Static.h"
#include "Include/Symbols/dol.h"
#include "Include/musyx/musyx.h"

/* Ghidra's base spellings, still used by the recovered types below. */
#ifndef MSSB_GHIDRA_BASE_TYPES
#define MSSB_GHIDRA_BASE_TYPES
typedef unsigned char  byte;
typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned char      undefined;
typedef unsigned char      undefined1;
typedef unsigned short     undefined2;
typedef unsigned int       undefined4;
typedef unsigned long long undefined8;
#endif

/* ======================================================================== */
/*  1. types the decomp does not model yet                                  */
/* ======================================================================== */

typedef struct RawInputStruct RawInputStruct, *PRawInputStruct;
typedef struct challengeRostersStruct challengeRostersStruct, *PchallengeRostersStruct;
typedef struct Legacy_ChemistryTable Legacy_ChemistryTable, *PChemistryTable;
typedef unsigned char EnumCharacterClass;
typedef unsigned int EnumFieldingAbilities;
typedef struct Legacy_StatTable Legacy_StatTable, *PStatTable;
typedef struct Legacy_CharacterStats Legacy_CharacterStats, *PCharacterStats;
typedef unsigned short enumControllerInput;
typedef struct controllerInputStruct controllerInputStruct, *PcontrollerInputStruct;
typedef struct Static_MSSB_Data Static_MSSB_Data, *PStatic_MSSB_Data;
typedef struct TextCharacter TextCharacter, *PTextCharacter;
typedef struct ScreenText ScreenText, *PScreenText;
typedef short enumFielderShort;
typedef unsigned char EnumStadiumIDs;
typedef struct structCharSelect structCharSelect, *PstructCharSelect;
typedef struct StatisticsBatter StatisticsBatter, *PStatisticsBatter;
struct RawInputStruct { unsigned char _opaque_stub; } __attribute__((packed));
struct challengeRostersStruct {
    byte charID[9];
    undefined field1_0x9;
    undefined field2_0xa;
    undefined field3_0xb;
    undefined field4_0xc;
    undefined field5_0xd;
    undefined field6_0xe;
    undefined field7_0xf;
    undefined field8_0x10;
    undefined field9_0x11;
    byte characterTypes[9];
    undefined field11_0x1b;
    undefined field12_0x1c;
    undefined field13_0x1d;
    undefined field14_0x1e;
    undefined field15_0x1f;
    undefined field16_0x20;
    undefined field17_0x21;
    undefined field18_0x22;
    undefined field19_0x23;
    undefined field20_0x24;
    undefined field21_0x25;
    undefined field22_0x26;
    undefined field23_0x27;
    undefined field24_0x28;
    undefined field25_0x29;
    undefined field26_0x2a;
    undefined field27_0x2b;
    undefined field28_0x2c;
    undefined field29_0x2d;
    undefined field30_0x2e;
    undefined field31_0x2f;
    undefined field32_0x30;
    undefined field33_0x31;
    undefined field34_0x32;
    undefined field35_0x33;
    undefined field36_0x34;
    undefined field37_0x35;
    undefined field38_0x36;
    undefined field39_0x37;
    undefined field40_0x38;
    undefined field41_0x39;
    undefined field42_0x3a;
    undefined field43_0x3b;
    undefined field44_0x3c;
    undefined field45_0x3d;
    undefined field46_0x3e;
    undefined field47_0x3f;
    undefined field48_0x40;
    undefined field49_0x41;
    undefined field50_0x42;
    undefined field51_0x43;
    undefined field52_0x44;
    undefined field53_0x45;
    undefined field54_0x46;
    undefined field55_0x47;
} __attribute__((packed));
struct Legacy_ChemistryTable {
    byte Mario;
    byte Luigi;
    byte DK;
    byte Diddy;
    byte Peach;
    byte Daisy;
    byte Yoshi;
    byte BabyMario;
    byte BabyLuigi;
    byte Bowser;
    byte Wario;
    byte Waluigi;
    byte RedKoopa;
    byte RedToad;
    byte Boo;
    byte Toadette;
    byte RedShyGuy;
    byte Birdo;
    byte Monty;
    byte BowserJr;
    byte RedParatroopa;
    byte BluePianta;
    byte RedPianta;
    byte YellowPianta;
    byte BlueNoki;
    byte RedNoki;
    byte GreenNoki;
    byte HammerBro;
    byte Toadsworth;
    byte BlueToad;
    byte YellowToad;
    byte GreenToad;
    byte PurpleToad;
    byte BlueMagikoopa;
    byte RedMagikoopa;
    byte GreenMagikoopa;
    byte YellowMagikoopa;
    byte KingBoo;
    byte Petey;
    byte Dixie;
    byte Goomba;
    byte Paragoomba;
    byte GreenKoopa;
    byte GreenParatroopa;
    byte BlueShyGuy;
    byte YellowShyGuy;
    byte GreenShyGuy;
    byte BlackShyGuy;
    byte GrayDryBones;
    byte GreenDryBones;
    byte RedDryBones;
    byte BlueDryBones;
    byte FireBro;
    byte BoomerangBro;
} __attribute__((packed));
struct Legacy_StatTable {
    byte CurveBallSpeed;
    byte FastBallSpeed;
    byte cursedBall;
    byte Curve;
    byte curveControl;
    byte UnusedBytes[27];
    EnumFieldingAbilities FieldingStats;
    short CharID;
    byte FieldingArm;
    byte BattingStance;
    byte SlapContactSize;
    byte ChargeContactSize;
    byte SlapHitPower;
    byte ChargeHitPower;
    byte BuntingContactSize;
    byte HitTrajectoryPushPull;
    byte HitTrajectoryHighLow;
    byte Speed;
    byte ThrowingArm;
    EnumCharacterClass CharacterClass;
    byte Weight;
    byte Captain;
    byte CaptainStarHitPitch;
    byte NonCaptainStarSwing;
    byte NonCaptainStarPitch;
    byte BattingStatBar;
    byte PitchingStatBar;
    byte RunningStatBar;
    byte FieldingStatBar;
} __attribute__((packed));
struct Legacy_CharacterStats {
    struct Legacy_StatTable Stats;
    struct Legacy_ChemistryTable Chemistry;
    byte BytesAfterChemistry[3];
    short UnusedShorts[22];
} __attribute__((packed));
struct controllerInputStruct {
    enumControllerInput currentHeldInput;
    enumControllerInput newInput;
    enumControllerInput processedInput;
} __attribute__((packed));
struct Static_MSSB_Data {
    struct Legacy_CharacterStats CharacterData[54];
    byte field1_0x21c0[8640];
    struct challengeRostersStruct challengeRosters[11];
    struct challengeRostersStruct bjChallengeRoster;
    int captainSelectedID[2];
    void * field5_0x46e8[4];
    byte playerNumberByPort[4];
    byte portsActiveInMatch[4];
    byte field8_0x4700;
    byte field9_0x4701;
    byte field10_0x4702;
    byte field11_0x4703;
    byte field12_0x4704;
    byte field13_0x4705;
    byte field14_0x4706;
    byte field15_0x4707;
    byte mainMenuOptionSelectedIndex;
    byte capLocationInOrder_P1_P2_[2];
    byte field18_0x470b[2];
    byte teamName_P1_P2_[2];
    byte startingChemStars[2];
    byte field21_0x4711;
    byte field22_0x4712;
    byte field23_0x4713[13];
    int field24_0x4720[2];
    byte mode;
    byte player2Ind_;
    byte field27_0x472a;
    byte field28_0x472b;
    struct controllerInputStruct controllerInputs[2];
    byte field30_0x4738[16];
    void * (*field31_0x4748)[0];
    void * (*field32_0x474c)[0];
    byte field33_0x4750;
    byte field34_0x4751;
    byte field35_0x4752[2];
    byte field36_0x4754;
    byte field37_0x4755;
    byte field38_0x4756;
    byte charOnCharacterGridSelected[54];
    byte battingOrderIndex[9];
    byte field41_0x4796;
    byte field42_0x4797;
    byte field43_0x4798;
    byte field44_0x4799;
    byte field45_0x479a;
    byte field46_0x479b;
    byte field47_0x479c;
    byte field48_0x479d;
    byte field49_0x479e;
    byte field50_0x479f;
    byte field51_0x47a0;
    byte field52_0x47a1;
    byte field53_0x47a2;
    byte field54_0x47a3;
    byte field55_0x47a4;
    byte field56_0x47a5;
    byte field57_0x47a6;
    byte field58_0x47a7;
    byte field59_0x47a8;
    byte field60_0x47a9;
    byte field61_0x47aa;
    byte field62_0x47ab;
    byte field63_0x47ac;
    byte field64_0x47ad;
    byte field65_0x47ae;
    byte field66_0x47af;
    byte field67_0x47b0;
    byte field68_0x47b1;
    byte field69_0x47b2;
    byte field70_0x47b3;
    byte field71_0x47b4;
    byte field72_0x47b5;
    byte field73_0x47b6;
    byte field74_0x47b7;
    byte field75_0x47b8;
    byte field76_0x47b9;
    byte field77_0x47ba;
    byte field78_0x47bb;
    byte field79_0x47bc[61];
    byte field80_0x47f9[93];
    byte field81_0x4856[68];
    byte field82_0x489a;
    byte charIsStarred[9];
    undefined field84_0x48a4;
    undefined field85_0x48a5;
    undefined field86_0x48a6;
    undefined field87_0x48a7;
    undefined field88_0x48a8;
    undefined field89_0x48a9;
    undefined field90_0x48aa;
    undefined field91_0x48ab;
    undefined field92_0x48ac;
    byte field93_0x48ad;
    byte field94_0x48ae;
    byte field95_0x48af;
    byte field96_0x48b0;
    byte field97_0x48b1;
    byte field98_0x48b2;
    byte field99_0x48b3;
    byte field100_0x48b4[84];
} __attribute__((packed));
struct TextCharacter {
    byte flag_row_;
    byte character_col_;
} __attribute__((packed));
struct ScreenText {
    byte *field0_0x0;
    struct TextCharacter *currentCharPtr_;
    s32 color;
    int field3_0xc;
    ushort xPos;
    ushort yPos;
    short field6_0x14;
    short field7_0x16;
    short field8_0x18;
    short field9_0x1a;
    short field10_0x1c;
    short field11_0x1e;
    short field12_0x20;
    short field13_0x22;
    short field14_0x24;
    short field15_0x26;
    short currLetterBeingDrawn;
    byte field17_0x2a;
    byte field18_0x2b;
    byte textStyle_;
    byte field20_0x2d;
    byte lineSpacing_;
    byte alighment_left_center_right;
    byte alignment_middle_top_bottom;
    byte field24_0x31;
    short field25_0x32;
    byte completedWritingSentenceInd_;
    byte completedWritingSentenceInd_2;
    byte field28_0x36;
    byte field29_0x37;
} __attribute__((packed));
struct structCharSelect {
    byte rosterCharID[2][9];
    byte positionSwapMapping[2][9];
    byte chemWCaptain[2][9];
    byte unused[2][9];
    byte rosterSpotFilledInd[2][9];
} __attribute__((packed));
struct StatisticsBatter {
    short field0_0x0;
    byte onFieldForAPitch;
    byte plateAppearances;
    byte AtBats;
    byte Hits;
    byte Singles;
    byte Doubles;
    byte Triples;
    byte HomeRuns;
    byte BuntSuccesses;
    byte SacFlies;
    byte GIDP;
    byte Strikeouts;
    byte Walks_4Balls;
    byte Walks_Hit;
    byte RBI;
    byte runs;
    byte BasesStolen;
    byte AB_W_RISP_;
    byte hits_W_RISP;
    byte runnersAdvancedAgainst_runnersAllowedToAdvanceOnError_;
    byte fielder_playsInvolvedIn_opportunities__;
    byte RBI_W_RISP;
    byte HR_W_RISP;
    byte field24_0x19;
    byte currentPosition[9];
    byte BigPlays;
    byte StarHitsActivated;
    byte field28_0x25;
} __attribute__((packed));

/* ======================================================================== */
/*  2. objects the decomp names and addresses but does not type             */
/* ======================================================================== */

#define Static_Stats_Tables    VAR_ADDRESS(Static_MSSB_Data, Static_Stats_Tables_ADDR)
#define screenTextArray        ARRAY_1D_ADDRESS(ScreenText, 30, screenTextArray_ADDR)

/* Not in the decomp's symbol table at all -- addresses kept from the export. */
#define controllerInputs__ADDR 0x803530CC
#define mapCaptainCursorPositionToCharID ARRAY_1D_ADDRESS(byte, 12, 0x800FE5D4)

/* A member of the old struct_matchInfo_ that the decomp's GameInitVariables
   does not declare. The preprocessor cannot rewrite `x.field`, so this is a
   standalone accessor and the call sites use it directly. */
#define player2Ind2__ADDR      0x803C5EA9

/* ======================================================================== */
/*  3. fields the old export flattened to absolute addresses                */
/*                                                                          */
/*  The comment on each line is where the decomp actually puts it, from      */
/*  docs/rename_map.md. Reach for the decomp's own struct when you can.      */
/* ======================================================================== */

#define InputBuffer                  ARRAY_1D_ADDRESS(PADStatus, 4, 0x802E9F40)   // lbl_802E9F20 + 0x20
#define teamManagement_cursorPos     ARRAY_1D_ADDRESS(byte, 2, 0x80336726)        // aiPosSwapInputs + 0xCF46
#define Roster_Player1               VAR_ADDRESS(structCharSelect, 0x803C6726)    // cursorPositions + 0x2
#define BatterStats_P1_P2_           ARRAY_2D_ADDRESS(StatisticsBatter, 2, 9, 0x803537E4)  // Static_Stats_Tables + 0x4E44
#define AtBat_PitchThrown            VAR_ADDRESS(byte, 0x8088A81B)                // g_Stats + 0x37

/* ======================================================================== */
/*  4. enum constants with no decomp counterpart                            */
/*                                                                          */
/*  From docs/enum_map.md. Per the import policy these belong in the decomp  */
/*  first (Ghidra -> decomp -> here), so treat them as on loan.              */
/* ======================================================================== */


#ifndef STADIUM_ID_TOY_FIELD
#define STADIUM_ID_TOY_FIELD                                  6
#endif

/* ======================================================================== */
/*  5. functions the decomp names and addresses but never prototypes        */
/*                                                                          */
/*  Include/Symbols gives the address; there is no signature anywhere, so    */
/*  these are unprototyped function pointers -- the compiler will not check  */
/*  the arguments, and an `int` return is assumed. Correct when the result   */
/*  is an integer or ignored; if you need a real signature, write the        */
/*  FUNCTION_ADDRESS call out at the call site instead.                      */
/* ======================================================================== */

#define characterSelectScreen        ((int (*)())characterSelectScreen_ADDR)
#define randRange_FUN_80042bf0       ((int (*)())randRange_FUN_80042bf0_ADDR)
#define selectRandomStadium          ((int (*)())selectRandomStadium_ADDR)
#define setCaptainLocInRoster        ((int (*)())setCaptainLocInRoster_ADDR)
#define setPortOfEachPlayer          ((int (*)())setPortOfEachPlayer_ADDR)
#define teamLogoDetermination        ((int (*)())teamLogoDetermination_ADDR)
#define transferStatsToInMemRoster   ((int (*)())transferStatsToInMemRoster_ADDR)
#define unsure_FillRosterPositions   ((int (*)())unsure_FillRosterPositions_ADDR)
