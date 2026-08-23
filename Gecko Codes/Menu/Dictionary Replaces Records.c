/*###########################################################
# Dictionary Replaces Records
###########################################################*/
// Author: LittleCoaks
//
// *Makes the main-menu "Records" button open the game's unused Dictionary scene instead of Records (present in JP version).
//
// This is the clean, stock-scene version -- it only reroutes and fixes the
// music. For the custom scene with added elements, use "Dictionary Scene".

#include "Include/Rio/DictionaryReroute.h"

// No .address -> runs every frame; .state = MSSB_MENU -> only while the menu
// REL is resident.
CGECKO(DictionaryReplacesRecords, .state = MSSB_MENU);
void DictionaryReplacesRecords()
{
    DictionaryReroute_Tick();
}
