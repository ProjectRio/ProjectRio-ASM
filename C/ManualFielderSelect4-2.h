#include "helpers.h"
#include "fielder_structs.h"

typedef enum mfsStateEnum {
  NO_ACTION = 0,
  SELECT_CLOSEST = 1,
  SELECT_PITCHER = 2,
  SELECT_CATCHER = 3
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

#define current_mfs_state value_atAddr(byte, 0x802EBF97)
#define mfs_fielder value_atAddr(byte, 0x802EBF99)

#define fielder_inputs value_atAddr(halfword, 0x8089289A) // R: 0x20, L: 0x40, Y: 0x800, X: 0x400
#define hand_fielder_ID value_atAddr(byte, 0x80892801)

#define frames_after_contact value_atAddr(halfword, 0x8089269E)
#define outs value_atAddr(byte, 0x80892973)

#define ball_state value_atAddr(byte, 0x80892701)
#define type_of_swing value_atAddr(byte, 0x8089099B)
// #define ball_contact_result value_atAddr(short, 0x808926B2) // 0: in air, 1: landed, 2: fielded, 3: caught, -1: foul
// #define fielder_lockout_array array_atAddr(byte, 0x807B5CF8)

//#define drop_spot_x value_atAddr(float, 0x80890E80)
//#define drop_spot_z value_atAddr(float, 0x80890E84)
#define ball_x value_atAddr(float, 0x80890B38)
#define ball_z value_atAddr(float, 0x80890B40)
