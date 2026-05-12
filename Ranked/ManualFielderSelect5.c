/*#########################################################
Manual Fielder Select v5.0
Author: PeacockSlayer, LittleCoaks
#########################################################*/


// *MECHANICS:
// *R - closest fielder to ball that's not currently selected
// *L - undo manual fielder select action

// Address: 0x80678F8C
// Instruction: lbz r0, 0x1BD1(r6) (do i need?)
 
#include "ManualFielderSelect5.h"

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

  // deselect fielder
  if (current_mfs_state == NO_ACTION)
  {
    RESTORE_ALL;
    __asm__("lbz 0, 0x1BD1(6)"); // instruction that was overwriten for gecko code
    return;
  }

  // select closest fielder
  else if (current_mfs_state == SELECT_CLOSEST)
  {
    if (R_pressed_this_frame) // if R pressed, calculate closest fielder, else select whoever was closest last time R was pressed
    {
      byte fielder_to_select = 0;
      bool min_distance_set = false;
      float min_distance;
      for (int i = 0; i < LEN(FielderData); i++)
      {
        // distance^2 == (x1 - x2)^2 + (y1 - y2)^2
        float distance = SQUARE(ball_x - FielderData[i].pos.X) + SQUARE(ball_z - FielderData[i].pos.Z);
        byte fielder_status = FielderData[i].autoMovementFunctionIndex;
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
  
  __asm__("li 0, 1"); // in injected code, this tells game not to select a new fielder
}