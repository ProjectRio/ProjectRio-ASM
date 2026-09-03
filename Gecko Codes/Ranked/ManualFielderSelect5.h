/*#########################################################
ManualFielderSelect5.h -- symbols for the C port of MFS v5.0
#########################################################*/
// Addresses are the ones the shipping "Manual Fielder Select 5.asm" uses; the
// fielder array itself comes from the generated headers (g_Fielders, 9 x
// InMemFielder @0x8088F368) instead of the old hand-written fielder_structs.h.

#ifndef MANUAL_FIELDER_SELECT_5_H
#define MANUAL_FIELDER_SELECT_5_H

#include "Include/game/UnknownHomes_Game.h"

typedef enum mfsStateEnum {
  NO_ACTION = 0,
  SELECT_CLOSEST = 1
} mfsStateEnum;

typedef enum ballStateEnum {
  UNFIELDED = 0,
  FIELDER_HOLDING = 1,
  BALL_THROWN = 2,
  LOOSE_BALL = 3
} ballStateEnum;

typedef enum swingTypeEnum {
  SLAP_LDSS = 0,
  CHARGE_GROUNDSTAR_POPSTAR = 1,
  CAPTAINSTAR_MOONSHOT = 2,
  BUNT = 3
} swingTypeEnum;

// ---- claimed free memory (see ClaimedFreeMemory.h) ------------------------
#define current_mfs_state VAR_ADDRESS(u8, 0x802EBF97)
#define mfs_fielder VAR_ADDRESS(u8, 0x802EBF99) // MUST CHANGE ADDRESS, MEMORY CLAIMED

// ---- game state ----------------------------------------------------------
#define fielder_inputs VAR_ADDRESS(halfword, 0x8089289A) // R: 0x20, L: 0x40
#define hand_fielder_ID VAR_ADDRESS(u8, 0x80892801)

#define frames_after_contact VAR_ADDRESS(halfword, 0x8089269E)
#define outs VAR_ADDRESS(u8, 0x80892973)

#define ball_state VAR_ADDRESS(u8, 0x80892701)
#define type_of_swing VAR_ADDRESS(u8, 0x8089099B)
// #define ball_contact_result VAR_ADDRESS(short, 0x808926B2) // 0: in air, 1: landed, 2: fielded, 3: caught, -1: foul
// #define fielder_lockout_array ARRAY_1D_ADDRESS(u8, 9, 0x807B5CF8)

//#define drop_spot_x VAR_ADDRESS(float, 0x80890E80)
//#define drop_spot_z VAR_ADDRESS(float, 0x80890E84)
#define ball_x VAR_ADDRESS(float, 0x80890B38)
#define ball_z VAR_ADDRESS(float, 0x80890B40)

// The fielders' per-frame movement state. The ASM version pokes the same bytes
// through raw offsets: base 0x8088F368, stride 0x268, control status at +0x1D3.
#define FielderData g_Fielders
#define hasControl_goingToBall      AUTO_MOVEMENT_GOING_TO_BALL              // 0xF
#define trackHitBall_phase2_AITeam  AUTO_MOVEMENT_TRACK_HIT_BALL_PHASE2_AI_TEAM // 0x2

#endif
