#include "Common.h"

#define FielderData VAR_ADDRESS(InMemFielder, 9, 0x8088F368)

typedef struct InMemFielder InMemFielder, *PInMemFielder;
typedef struct Vec3f Vec3f, *PVec3f;
typedef struct Hitbox Hitbox, *PHitbox;

typedef enum fieldersEnum {
    Pitcher=0,
    Firstbaseman=1,
    Secondbaseman=2,
    Thirdbaseman=3,
    Shortstop=5,
    Leftfielder=6,
    Centerfielder=7,
    Rightfielder=8
} fielders;

typedef enum fielderLocationEnum {
    home=0,
    first_base=1,
    second_base=2,
    third_base=3,
    mound=5,
    outfieldCutoff=6,
    fielderDefaultPosition=7,
    unnknown_6_0=8 /* The ones above 8 are found at 807b5a84 */,
    unnknown_n6_0=9 /* Between the base coordinates and these, there's other info in between */,
    unnknown_20_27=10,
    unnknown_8_40=11,
    unnknown_n8_40=12,
    unnknown_n20_27=13,
    unknown14=14,
    unknown15=15,
    none=-1
} fielderLocationEnum;

typedef enum presetLocationCategory {
    RFFoulTerritory=1,
    outfieldChillSpots=2,
    LFFoulTerritory=3
} presetLocationCategory;

typedef enum EnumCharacterClass {
    Balanced=0,
    Power=1,
    Speed=2,
    Technique=3
} EnumCharacterClass;

typedef enum enumSpecialJump {
    wallSplat=1,
    wallJump=2,
    clamber=3
} enumSpecialJump;

typedef enum autoMovementFunctionIndex {
    stayStill=0,
    coveringBase=1,
    trackHitBall_phase2_AITeam=2,
    trackHitBall_phase1_AITeam=3,
    cutoffPhase2=5,
    outfielderNoCatch_phase2=6,
    outfielderNoCatch_phase1=7,
    outfielderNoCatch_phase1_=8,
    someKindOfDefault=9,
    hasBall=10,
    cutoff=11,
    stopBetweenInstructions=12,
    infieldSupport=14,
    hasControl_goingToBall=15,
    trackHitBall_phase2_humanTeam=16,
    runningOffField=17,
    trackHitBall_primaryFielder_humanTeam=21,
    trackHitBall_secondaryFielder_humanTeam=22,
    trackHitBall_tertiaryFielder_humanTeam=23,
    trackHitBall_secondaryOutfielderOnDeepFlyBall=24,
    readyToInterceptThrownBall=25
} autoMovementFunctionIndex;

typedef enum presetFielderLocations {    
    home_preset=0, // (0,0)
    home_v2_preset=1, // (0,0)
    shallow_first_preset=2, // (17,4)
    shallow_third_preset=3, // (-17,4)
    shallow_rf_preset=4, // (36,30)
    shallow_lf_preset=5, // (-36,30)
    right_center_preset=6, // (17,57)
    left_center_preset=7, // (-17,57)
    left_field_preset=8, // (-28,53)
    center_preset=9, // (0,68)
    right_field_preset=10 // (28,53)
} presetFielderLocations;

typedef enum fielderVeloAdjCode {
    noAdjustment=0,
    subtact0p0005=1,
    subtract0p001=2,
    subtract0p002=3,
    subtract0p003=4,
    subtract0p0005_pt2=5,
    subtract0p001_pt2=6,
    subtract0p002_pt2=7,
    subtract0p003_pt2=8,
    setTo0=9,
    setTo0p7=10,
    setTo0p1=11,
    setTo0p03=12,
    setTo0p25_runOffFieldAfterInning=13
} fielderVeloAdjCode;

typedef enum catchStrategy {
    catchFlyBall=1,
    potentialCatchFlyBall=2,
    potentialDivingCatch=3,
    ballOutOfRange=4
} catchStrategy;

typedef enum fielder0x1ffState {
    hasntCheckedIfOutfielderIsInitialCharacterSelected=0,
    checkedIfOutfielderIsInitialCharacterSelected=1,
    fielderCatchingBall=2
} fielder0x1ffState;

typedef enum clamberStatus {
    noClamber=0,
    jumpingOntoWall=1,
    onwall=2,
    jumpingOffWall=3,
    unknown1=4,
    unknown2=5
} clamberStatus;

typedef enum wallSplatStatus {
    noWallSplat=0,
    goingOntoWall=1,
    onWall=2,
    slidingDownWall=3,
    onGroundGroggy=4
} wallSplatStatus;

typedef enum knockOutStatus {
    notKnockedOut=0,
    knockedOut=1,
    knockOutEnded=2
} knockOutStatus;

typedef enum bodyCheckResult {
    noBodyCheck=0,
    fail=1,
    success=2
} bodyCheckResult;

typedef enum bodyCheckStatusFielder {
    noBodyCheckFielder=0,
    started=1,
    failedOneEnding=2,
    successfulOneEnding=3
} bodyCheckStatusFielder;

typedef enum CatchAnimationType {
    noCatchAnimation=0,
    catchBattedBall=1,
    catchThrow=2,
    divingCatch=3,
    WallJump=4,
    clamberCatch=5,
    JumpingCatch=6,
    runningCatch=7,
    coverBaseLungingCatch=8
} CatchAnimationType;

typedef enum EnumBobbleType {
    Caught_FumbleCheck=0,
    Caught_BobbleCheck=1,
    Fumbled=2,
    Bobbled=3,
    Fireball=4
} EnumBobbleType;

typedef enum EnumFielderAction {
    noFielderAction=0,
    Bobbling=1,
    Sliding=2,
    Walljumping=3,
    stopedMoving=19
} EnumFielderAction;

typedef enum wallJumpStatus {
    noWallJump=0,
    walljumpingOntoWall=1,
    contactingWall=2,
    jumpedOffWall=3,
    landed=4
} wallJumpStatus;

struct Vec3f {
    float X;
    float Y;
    float Z;
};

struct Hitbox {
    float RadiusAirOrFast;
    float RadiusGround;
    float centredCatchRadius;
    float Height;
    float highCatchThreshold;
    float lowCatchThreshold;
    float unknown;
    float DiveRange;
    float infieldDiveHeight;
};

struct InMemFielder { /* in ram view of a fielder */
    struct Vec3f pos;
    float actionYOffset;
    float storedPosY;
    struct Vec3f IntendedLocation;
    float maybeTargetPosX;
    float maybeTargetPosZ;
    float xPos5mAwayFromBallsCollisionSpotOnWall;
    float zPos5mAwayFromBallsCollisionSpotOnWall;
    float velocityX;
    float velocityZ;
    float xMovementDir;
    float zMovementDir;
    float velocityXPrev;
    float velocityZPrev;
    float desiredMovementDirection;
    float desiredMovementDirectionStored;
    float currentVelocity;
    float lastFrameVelocityUpToBase;
    float joggingSpeed;
    float runningAccelerationFactor;
    float unused;
    float desiredMovementDirection2;
    float distanceFromAutoLocation;
    float wjAngleRelated;
    float distanceFromHomePlate;
    float distanceFromBall;
    float distanceFromBallLastFrame;
    float xDistanceFromBall;
    float distanceFromLandingSpot;
    float distanceFromEachFielder[9];
    float distanceToBases[4];
    float distanceToMound;
    float desiredMovementDirectionF1;
    float desiredMovementDirectionF2;
    float desiredMovementDirectionF3;
    float desiredMovementDirectionF4;
    float desiredMovementDirectionF5;
    float unused2;
    float posXLastFrame;
    float posZLastFrame;
    float unused3;
    float unused4;
    float unused5;
    struct Hitbox hitbox;
    struct Vec3f throwTarget;
    float storedPosX;
    float storedPosZ;
    float knockBackRelatedVeloX;
    float knockbackRelatedVeloZ;
    float wallActionLocationX;
    float wallActionLocationY;
    float wallactionLocationZ;
    float wallJumpApexX;
    float wallJumpApexY;
    float wallJumpApexZ;
    float actionDirectionRadians;
    float wallActionFacingAngle;
    float wallActionCurrentHeight;
    float wallActionHeightAdjustment;
    struct Vec3f wallActionVelo;
    float jumpY;
    struct Vec3f jumpVelocity;
    float somethingHitboxRelated;
    float unused6;
    float knockOutVelo;
    short rosterLocation;
    short CharID;
    short unused_missionRelated;
    short numFramesToGetToAutoLocation;
    short playerAngleFromHome;
    short angleOfFieldersStartingPosition;
    short framesRemainingToGetToLandingSpot;
    short framesToGetToBallLandingSpot;
    short field73_0x188;
    short AIDistToStandFromWallCollision;
    short locationResponsibleForCovering; //enum fielderLocationEnum
    short baseCuttoffIsTargeting;
    short presetLocationCategory; // enum presetLocationCategory
    short throwWindupEstimate;
    short framesSinceThrowWasMade;
    short const_60;
    short field81_0x198;
    short runningAngle;
    short fielderReadiness_ALways0;
    short movementAngle;
    short framesSinceStartedMoving;
    short someCountdownAndCountUpRelatedToStandingStill;
    short unknown_writeOnly_always0;
    short field88_0x1a6;
    short someCountDown;
    short field90_0x1aa;
    short timeSinceThrowWasCaught;
    short jumpCountDown;
    short jumpApexFrame;
    short specialActionCountdown;
    short wallSplatStageCountDown;
    short someCountDown2;
    short onFireCountUp;
    short onFireCountdown;
    short onFireFacingAngle;
    short knockOutCountUp;
    short knockOutCountDown;
    short knockOutAngle;
    byte AILevel3Weak0Powerful;
    byte AI_Ind;
    byte autoFielderInd;
    byte throwingHandedness;
    byte characterClass; // enum EnumCharacterClass
    byte Weight;
    byte ModifiedWeightForMag;
    byte wallActionAbility; // enum enumSpecialJump
    byte hasSuperJump;
    byte modifiedThrowingArm;
    byte throwingArm;
    byte movementSpeedFactor;
    byte speed;
    byte maxAccLength_ConstF;
    byte lockoutDuration;
    byte autoMovementFunctionIndex; // enum autoMovementFunctionIndex
    byte presetLocationToStandAt; // enum presetFielderLocations
    byte unknown_writeOnly;
    byte fielderVeloAdjustmentCode; // enum fielderVeloAdjCode
    bool isResponsibleForCoveringALocation;
    byte pitcherHeadingToCover1stOr3rd;
    byte goingToAutoLocationInd;
    byte cantCatchFlyBallInd;
    byte nonCatchFlyBallStratInd;
    byte maybeMovementState;
    byte catchStrategy; // enum catchStrategy
    byte inBasePath;
    byte fielderMadeThrow;
    byte field131_0x1e0;
    byte field132_0x1e1;
    byte lockedOutInd;
    byte standingStillInd;
    byte field135_0x1e4;
    byte animationRelated;
    byte terrainOrCollisionRelated;
    byte always0_forcePlayRelated;
    byte unknown_Unused;
    byte playerNeedsToMoveToCatchThrowInd;
    byte unused_alwaysSetTo0;
    byte field142_0x1eb;
    byte animationRelatedInd;
    byte animatingActionInd;
    byte hitKnockbackCountdown;
    byte stunFramesOnFireBall;
    byte unused_jumpActiveOrRunningCatchRelated;
    byte field148_0x1f1;
    byte field149_0x1f2;
    byte runningVsLookingAngleCode;
    byte field151_0x1f4;
    byte baseCurrentlyOn;
    byte baseOnLastFrame;
    byte baseOn2;
    byte outFieldZoneCode;
    byte field156_0x1f9;
    byte atDugouotAtEndOfInning;
    bool field158_0x1fb;
    byte wallActionFacingAngleInd;
    byte relatedToStandingStill;
    byte field161_0x1fe;
    byte fielderTrackingBallState; // enum fielder0x1ffState
    byte field163_0x200;
    byte fielderIsInitialOutfielderSelectedInd;
    byte cutoffWaitingToInterceptThrownBall;
    bool isJump;
    byte jumpCountUp;
    byte clamberStatus; // enum clamberStatus
    byte wjRelated;
    byte wallSplatStatus; // enum wallSplatStatus
    byte field171_0x208;
    byte field172_0x209;
    byte field173_0x20a;
    byte field174_0x20b;
    byte ox20b_stored;
    byte field176_0x20d;
    byte field177_0x20e;
    byte onFire;
    byte knockoutStatus; // enum knockOutStatus
    byte bodyCheckResult; // enum bodyCheckResult
    byte bodyCheckStatus; // enum bodyCheckStatusFielder
    byte bodyCheckRunnerNumber;
    byte bodyCheckBase;
    byte throwWindUpFrames;
    byte needToMoveToCatchThrownBall;
    byte dkJungleRelatedCountdown;
    byte field187_0x218;
    byte field188_0x219;
    byte field189_0x21a;
    byte field190_0x21b;
    struct Vec3f actionStartingCoordinate;
    struct Vec3f fielderVelocityDuringAction;
    short actionEndingCoordinateX;
    short field194_0x236;
    float diveEndingCoordinateY;
    float actionEndingCoordinateZ;
    float xDistToCatch;
    float zDistToCatch;
    float distToWhereFlyBallWillBeAtHalfFielderHeight;
    short catchAnimationFramesCountDown;
    short catchAnimationFramesCountUp;
    short always0;
    byte catchAnimation; // enum CatchAnimationType
    byte catchVerticalZone;
    byte catchCentreRightLeftOfBody;
    byte catchFastBattedBallInd;
    byte closingInOnCatchingFlyBall;
    byte closingInOnCatchingFlyBall_stored;
    byte bobble; // enum EnumBobbleType
    byte action; // enum EnumFielderAction
    byte jumpDiveStateRelated;
    byte autoCatch0_NoCatch1_OnlyAnimation;
    byte wallJumpFramesTillTopOfWallContact;
    byte wallActionFrameCounter;
    byte wallJumpStatus; // enum wallJumpStatus
    byte wallActionCountDown;
    byte runningCatchInd;
    byte actionAngleType;
    byte runningCatchCountDown;
    byte field220_0x263;
    byte caughtBallInAir;
    byte suctionCatchInd;
    byte field223_0x266;
    byte field224_0x267;
};