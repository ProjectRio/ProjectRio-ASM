/*###########################################################
# RioModPack
###########################################################*/
// Author: LittleCoaks
//
// *One gecko code holding the mod-options menu and every mod it can toggle.
// *The stock mods are NOT modified and NOT re-declared here -- each is pulled
// *in as a source with a gate address set around the #include, and cgecko
// *wraps all of that file's hooks in a runtime conditional by itself.
//
// Each mod still builds and ships standalone exactly as before: with no
// CGECKO_GATE_ADDR set, the gate fields are 0 and cgecko emits the plain code.
//
// The gate is a gecko `20` (32-bit if-equal) around the WHOLE code, not an
// early return inside the body. That matters: a gated-off C2 is never applied,
// so its .instruction never runs either and the game is left genuinely stock.
// It is the only way to switch off a REPLACE hook (.instruction = "blr"), and
// it works for raw ASM() codes, which have no C body to return from.
//
// ADDING A MOD: give it an id in Include/Rio/ModOptions.h, add a row to
// s_options in Options Menu.c, and add three lines below. Nothing else.
#include "Include/game/UnknownHomes_Game.h"
#include "Include/Rio/ModOptions.h"

// ---- the options UI itself, never gated -- it is how you reach the toggles --
#include "Gecko Codes/Menu/Options Menu.c"

// ---- toggleable mods -------------------------------------------------------
#undef  CGECKO_GATE_ADDR
#define CGECKO_GATE_ADDR MODOPT_ADDR(MODOPT_CPU_SPRINT)
#include "Gecko Codes/Match/CPU Always Sprints.c"

#undef  CGECKO_GATE_ADDR
#define CGECKO_GATE_ADDR 0
