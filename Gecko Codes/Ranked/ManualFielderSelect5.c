/*#########################################################
Manual Fielder Select v5.0
Author: PeacockSlayer, LittleCoaks
#########################################################*/


// *MECHANICS:
// *R - closest fielder to ball that's not currently selected
// *L - undo manual fielder select action

// -------------------------------------------------------------------------
// STATUS: reference port only -- the SHIPPING code is
// "Manual Fielder Select 5.asm" in this folder. Keep this one disabled.
//
// Why it cannot replace the .asm: the ASM version ends by forcing r0 to 1,
// which is how it tells the game "I already picked a fielder, don't pick
// another" (the injection site is `lbz r0, 0x1BD1(r6)` and the game branches
// on that r0). A C code cannot set r0: cgecko's wrapper does `mflr r0` on
// entry and `lwz r0 / mtlr r0` on the way out, so it owns r0 across the body
// (see the Common.h register notes -- WRITE_GAME_REG only reaches r3-r31).
// So this port re-runs the overwritten load through .instruction and then
// lets the game choose normally; the selection it makes still lands, but the
// game is free to override it on the same frame.
// -------------------------------------------------------------------------

#include "ManualFielderSelect5.h"

#include "Include/Local/Legacy.h"
#include "Include/Local/LegacyGame.h"
CGECKO(Manual_Fielder_Select_5, .address = 0x80678F8C, .state = MSSB_GAME,
                               .instruction = "lbz r0, 0x1BD1(r6)");
void Manual_Fielder_Select_5()
{
  bool R_pressed_this_frame = false;

  // conditions to be met before proceeding with function
  if (frames_after_contact > 15 && (ball_state == UNFIELDED || ball_state == LOOSE_BALL || outs == 3)) // allowing MFS after 3 outs permits "moonwalking" which isn't necessary it's just really fun to do lol
  {
    // select fielder
    if (fielder_inputs & 0x20) // R pressed
    {
      R_pressed_this_frame = true;
      current_mfs_state = SELECT_CLOSEST;
    }
    else if (fielder_inputs & 0x40) // L pressed
    {
      current_mfs_state = NO_ACTION;
    }
  }
  else
  {
    current_mfs_state = NO_ACTION;
  }

  // deselect fielder -- .instruction re-runs the overwritten load, so the game
  // carries on with its own fielder pick
  if (current_mfs_state == NO_ACTION)
  {
    return;
  }

  // select closest fielder
  else if (current_mfs_state == SELECT_CLOSEST)
  {
    if (R_pressed_this_frame) // if R pressed, calculate closest fielder, else select whoever was closest last time R was pressed
    {
      u8 fielder_to_select = 0;
      bool min_distance_set = false;
      float min_distance;
      for (int i = 0; i < (int)LEN(FielderData); i++)
      {
        // distance^2 == (x1 - x2)^2 + (y1 - y2)^2
        float distance = SQUARE(ball_x - FielderData[i].pos.x) + SQUARE(ball_z - FielderData[i].pos.z);
        u8 fielder_status = FielderData[i].autoMovementFunctionIndex;
        if (fielder_status == hasControl_goingToBall) // don't run calc on this fielder
        {
          FielderData[i].autoMovementFunctionIndex = trackHitBall_phase2_AITeam;
        }
        else if (!min_distance_set)
        {
          min_distance_set = true;
          min_distance = distance;
          fielder_to_select = i;
        }
        else if (distance < min_distance)
        {
          min_distance = distance;
          fielder_to_select = i;
        }
      }
      mfs_fielder = fielder_to_select;
    }
    // select fielder
    FielderData[mfs_fielder].autoMovementFunctionIndex = hasControl_goingToBall;
    hand_fielder_ID = mfs_fielder;
  }
}
