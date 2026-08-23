/*###########################################################
# LegacyGame.h -- Ghidra-era names for match-REL objects
###########################################################*/
// Hand-written; the companion to Legacy.h for things that only exist while the
// match REL is resident. Including this file puts the mod in game context --
// Include/Symbols/game.h will refuse to coexist with the menus bindings, which
// is the point: the two RELs share one arena slot.
//
// Same rule as Legacy.h: the address comes from the decomp, the type comes from
// the old Ghidra export because the decomp does not model these yet. Delete
// entries here as the decomp grows real declarations for them.
#pragma once

#include "CGecko/Common.h"
#include "Include/mssbTypes.h"
#include "Include/types.h"
#include "Include/Local/Legacy.h"
#include "Include/Symbols/game.h"

/* ---- types the decomp does not model yet ------------------------------- */

typedef unsigned char enum_fielding0x113;
typedef unsigned char fielderTagAnimation;
typedef unsigned char enumStickDirection;
typedef unsigned char bodyCheckResult;
typedef unsigned char pickOffSteal;
typedef unsigned char throwSpeedType;
typedef unsigned char throwSPeedAlgoInd;
typedef unsigned short EnumControllerInput;
typedef unsigned char enumErrorType;
typedef struct Vec3f Vec3f, *PVec3f;
typedef unsigned char autoMovementCodeEnum;
typedef struct unkInputRelated unkInputRelated, *PunkInputRelated;
typedef unsigned char dashState;
typedef struct fielderDash fielderDash, *PfielderDash;
typedef unsigned short fielderLocationEnum;
typedef struct fielderDashStruct fielderDashStruct, *PfielderDashStruct;
typedef unsigned char enum_bigPlayCode;
typedef unsigned char enum_fielding0x127;
typedef struct globalFielding globalFielding, *PglobalFielding;
struct Vec3f {
    float X;
    float Y;
    float Z;
} __attribute__((packed));
struct unkInputRelated {
    short stickAngleWhenPressingA;
    byte aPressed_decidingWhatActionToTake;
    byte aiOutfieldFielderAction_;
    byte aiFieldingDashIndicator__[4];
} __attribute__((packed));
struct fielderDash {
    int const0_;
    int const_0_0_;
    float sprintSpeedMultiplier;
    short framesSinceLastDashInput;
    short sprintLengthInFrames;
    short chargeLevel;
    short sprintLengthInFramesStored;
    short dashingFielderIndex;
    short numberOfDashInputs;
    short postSprintSlowdownTimer;
    dashState sprintingState;
} __attribute__((packed));
struct fielderDashStruct {
    struct fielderDash fielderDashByPort[4];
    short pad;
    byte always0;
    byte padding;
    struct fielderDash *dashPtr;
    struct unkInputRelated specialActionChecks;
    byte field6_0x7c;
    byte field7_0x7d;
    byte field8_0x7e;
    byte pad2[3];
    byte field10_0x82;
    byte pad3[5];
    byte field12_0x88[4];
    struct unkInputRelated *jumpDiveStruct;
    struct Vec3f tagDirection;
    byte field15_0x9c[16];
    undefined field16_0xac;
    undefined field17_0xad;
    short playOverCounter;
    enumFielderShort selectedFielder;
    enumFielderShort secondaryFielder;
    enumFielderShort tertiaryFielder;
    short selectedFielder_stored;
    short secondaryFielderStored;
    short tertiaryFielderStored;
    short someFielderIndex;
    enumFielderShort cutoffFielderIndex;
    short someFrameCounterFielderRelated;
    short interceptThrowFielder;
    fielderLocationEnum locationThrownTo;
    short humanSelectedPlaceToThrow;
    short someHumanThrowFrameCounter;
    short locThrownTo2;
    short somethingForTryingTagOut_TargetBase_;
    short _0xcc_Stored;
    short fielderAssignedLocationIndex[7];  /* also labeled fielderCoveringBaseIndex */
    short runnerChasingAfter_;
    short runnerChasingAfter__stored;
    short someBase;
    short framesSince3rdOutWasMade;
    short throwWindUpFrameCounter;
    short runnerBeingTargettedForOut;
    short lastThrowingFielder;
    short framesRunnerIsOutBy_;
    short framesSincePlayEnded;
    short throwWindupFrames;
    short fieldersWhoCanCatchBall_bitIndicators_;
    short someRunnerNum;
    short bodyCheckStageCountdown;
    autoMovementCodeEnum fielderAutoMovementCode[9];
    byte baseCoveredInd[4];
} __attribute__((packed));
struct globalFielding {
    struct fielderDashStruct fielderControl;
    byte playerAtMoundCutoffLocation;
    throwSpeedType throwSpeedType;
    pickOffSteal liveBallBcOfPickoffOrStealCd;
    byte infieldFlyIndicator;
    byte birdoFarThrowInd_forAnimation;
    byte throwWindupAnimationDoneInd;
    byte someThrowSpeedIncreaseInd;
    undefined field8_0x10c;
    undefined field9_0x10d;
    byte playOverInd_;
    byte unused_always0__;
    byte hasProcessedFoulBall;
    fielderTagAnimation tagAnimationType_;
    byte tagResult_1_out_2_safe;
    enum_fielding0x113 processErrorCode_;
    byte fielderActionBeingProcessed;
    byte fielderActionBeingProcessed_prev;
    bodyCheckResult bodyCheckResult_;
    byte pitcher_inPitchingState;
    byte catcherNotFocusedOnRunnerScoring;
    throwSPeedAlgoInd setThrowSpeedTypeTo9Ind;
    byte unused_always0;
    byte index_runnerAttemptingBodycheck;
    byte always0_;
    undefined field25_0x11d;
    byte _2B_0__SS_1__whosHoldingOnRunnerAt2nd;
    byte quickThrowInd_;
    undefined field28_0x120;
    byte const__1;
    undefined field30_0x122;
    byte letFoulBallDropIfWinningRunOn3rdInd;
    byte tagAnimationCountdown;
    byte baseFielderIsOn_;
    s8 runnerTargetedOnThrowDuringSteal_Pickoff;
    enum_fielding0x127 _0x127_pickoff_steal_targetingRunnerStatusCode;
    byte canEndPlayOnLooseBallInd_;
    byte always0__;
    byte someCountDown;
    byte always0__2;
    byte ballWontBeControlledByFielderAnytimeSoonInd;
    byte _0smash1normalThrow;
    enumStickDirection stickAngleClassification;
    byte framesControlStickPointedInCertainQuadrant;
    byte throwWaitingInd_;
    byte field45_0x131;
    enumErrorType field46_0x132;
    enum_bigPlayCode bigPlayPotential;
    byte bigPlayFielderIndex;
    byte tagOutFirstFrameInd;
    byte field50_0x136;
    byte always0;
    byte field52_0x138;
    byte unkFlagMaybeInAir_;
    byte infielderSelectedOnPopFlyInd_;
    byte knockoutFinished_Processed_;
    byte throwInterceptionTriggered;
    byte smashThrowInd;
    byte IsChemistryThrow;
    bodyCheckResult bodyCheckResult2;
    byte laser_1;
    byte laser_2;
    byte field62_0x142;
    byte unused_always0_;
    byte field64_0x144;
    byte smashThrow_framesDirectionHeld_;
    byte _20FrameCycleCounter;
    byte field67_0x147;
    EnumControllerInput fielderInputs;
    EnumControllerInput fielderInputsLatestFrame;
    short unused_fielderControls0x8;
    undefined field71_0x14e;
    undefined field72_0x14f;
} __attribute__((packed));

/* ---- objects the decomp names and addresses but does not type ---------- */


/* The decomp models this object as g_FieldingLogic_s and does not declare a
   fielderControl member; the export modelled the same address as globalFielding
   whose first member IS fielderControl. Same bytes, two shapes -- this binding
   keeps the shape the mods were written against. */
#define g_FieldingLogicLegacy VAR_ADDRESS(globalFielding, 0x80892750)

/* Ghidra put teamIsCPU[2] at GameControlsStruct + 0x13E; the decomp declares
   baseRoundingState and overrun1st_doneChecking at those two bytes. The two
   disagree, and docs/field_map.md flags it -- this preserves what the mod was
   written to do rather than silently picking the decomp's reading. */
#define GameLogic_teamIsCPU   ARRAY_1D_ADDRESS(u8, 2, 0x80892ACA)  // g_GameLogic + 0x13E
