/*###########################################################
# LegacyMenus.h -- Ghidra-era names for menu-REL objects
###########################################################*/
// Companion to Legacy.h for things that only exist while the menu REL is
// resident. Including this puts the mod in menu context, so Include/Symbols
// will refuse to also bind the match REL -- the two share one arena slot.
#pragma once

#include "CGecko/Common.h"
#include "Include/mssbTypes.h"
#include "Include/Local/Legacy.h"
#include "Include/Symbols/menus.h"

/* ---- objects the decomp names and addresses but does not type ---------- */

#define cursorToStadIDMapping  ARRAY_1D_ADDRESS(u8, 10, cursorToStadIDMapping_ADDR)

/* ---- functions with an address but no prototype (see Legacy.h S5) ------ */

#define copyInfoToInMemRoster   ((int (*)())copyInfoToInMemRoster_ADDR)
#define changeScreenVariables   ((int (*)())changeScreenVariables_ADDR)
