/*#########################################################
# Captain Swap
# Author: nuche, LittleCoaks
###########################################################


MECHANICS:
- allows the player to change captains mid-draft

*/
#include "CaptainSwap.h"

// Address: 0x8064F674
// Value: 0x57C004E7

void CaptainSwap()
{
  // int team_id;
  // int player_inputs;
  // SET_VAR_REG(team_id, 27);
  // SET_VAR_REG(player_inputs, 30);
  register int team_id __asm__("27");
  register int player_inputs __asm__("30");
  BACKUP_ALL;

  if ((player_inputs & 0x1000) == 0)
  {
    RESTORE_ALL;
    return;
  }
  
  word cursor_position = roster_cursor_loc[team_id];
  if (is_player_card[team_id] || is_bottom_screen[team_id] || cursor_position >= 9)
  {
    PlaySound(0x1BA, volume, 0x3f, 0); // bad sound
    RESTORE_ALL;
    return;
  }

  int new_captain_roster_spot = -1;
  for (int i = 0; i < 9; i++)
  {
    if (position_mappings[team_id][i] == cursor_position)
    {
      new_captain_roster_spot = i;
    }
  }
  if (new_captain_roster_spot == -1)
  {
    PlaySound(0x1BA, volume, 0x3f, 0); // bad sound
    RESTORE_ALL;
    return;
  }
  byte new_captain = roster_arr[team_id][new_captain_roster_spot];

  bool is_captain_allowed = false;
  halfword captain_background_id;
  for (int i = 0; i < LEN(allowed_captains); i++)
  {
    if (allowed_captains[i] == new_captain)
    {
      is_captain_allowed = true;
      captain_background_id = captain_background_arr[i];
    }
  }
  if (!is_captain_allowed)
  {
    PlaySound(0x1BA, volume, 0x3f, 0); // bad sound
    RESTORE_ALL;
    return;
  }

  // update captain background
  // halfword captain_background_id;
  // if (new_captain == 0x00) {
  //   captain_background_id = 0x4A;
  // } else if (new_captain == 0x01) {
  //   captain_background_id = 0x5B;
  // } else if (new_captain == 0x04) {
  //   captain_background_id = 0x56;
  // } else if (new_captain == 0x05) {
  //   captain_background_id = 0x5C;
  // } else if (new_captain == 0x06) {
  //   captain_background_id = 0x58;
  // } else if (new_captain == 0x11) {
  //   captain_background_id = 0x5E;
  // } else if (new_captain == 0x0A) {
  //   captain_background_id = 0x57;
  // } else if (new_captain == 0x0B) {
  //   captain_background_id = 0x5D;
  // } else if (new_captain == 0x02) {
  //   captain_background_id = 0x59;
  // } else if (new_captain == 0x03) {
  //   captain_background_id = 0x5F;
  // } else if (new_captain == 0x09) {
  //   captain_background_id = 0x5A;
  // } else if (new_captain == 0x13) {
  //   captain_background_id = 0x60;
  // } else {
  //   captain_background_id = 0x3B;
  // }
  if (team_id)
  {
    captain_background_1 = captain_background_id;
  }
  else
  {
    captain_background_0 = captain_background_id;
  }

  // swap old and new captains
  captain_id[team_id] = new_captain;
  byte old_captain = roster_arr[team_id][0];
  roster_arr[team_id][0] = new_captain;
  roster_arr[team_id][new_captain_roster_spot] = old_captain;

  byte old_captain_position = position_mappings[team_id][0];
  position_mappings[team_id][0] = cursor_position;
  position_mappings[team_id][new_captain_roster_spot] = old_captain_position;
  
  // Access chemistry_table[new_captain] as a byte array to reference members by offset
  byte* captain_chem = (byte*)&chemistry_table[new_captain];
  for (int i = 1; i < 9; i++)
  {
    byte teammate_id = roster_arr[team_id][i];
    if (teammate_id != 0xff) // make sure roster spot has a character first
    {
      chem_with_captain_arr[team_id][i] = captain_chem[teammate_id];
    }
  }
  teamLogoDetermination(team_id);
  teamSelectionSetChemStars(unknown_ptr, team_id);
  updateCharacterSelectProcessCode(team_id, 0xf);
  PlaySound(0x1BC, volume, 0x3f, 0); // swish sound

  RESTORE_ALL;
}

