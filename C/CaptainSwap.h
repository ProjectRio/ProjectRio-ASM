#include "helpers.h"
#include "chemistry_table.h"

#define volume value_atAddr(int, 0x800EFBA4)
#define PlaySound function_atAddr(void, 0x800c836C, halfword, int, byte, byte) // sound id, volume, 0x3f, 0x0

#define roster_cursor_loc value_atAddr(word, 2, 0x80750C48)
#define is_player_card value_atAddr(bool, 2, 0x80750C8D)
#define is_bottom_screen value_atAddr(bool, 2, 0x80750C89)

#define position_mappings value_atAddr(byte, 2, 9, 0x803c6738)
#define roster_arr value_atAddr(byte, 2, 9, 0x803c6726)
#define allowed_captains value_atAddr(byte, 12, 0x80108ED0)

#define captain_id value_atAddr(word, 2, 0x80353080)
#define captain_background_0 value_atAddr(halfword, 0x803A488E)
#define captain_background_1 value_atAddr(halfword, 0x803A488E)

#define chem_with_captain_arr value_atAddr(byte, 2, 9, 0x803c674b)
#define unknown_ptr value_atAddr(int, 0x803530ec)

#define teamLogoDetermination function_atAddr(void, 0x800678cc, int)
#define teamSelectionSetChemStars function_atAddr(void, 0x806b4c78, int, int)
#define updateCharacterSelectProcessCode function_atAddr(void, 0x800625A4, int, int)

byte captain_background_arr[12] = {0x4A, 0x5B, 0x56, 0x5C, 0x58, 0x5E, 0x57, 0x5D, 0x59, 0x5F, 0x5A, 0x60};