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
// ADDING A MOD: give it an id in RioModPack/ModOptions.h, add a row to
// s_options in Options Menu.c, and add three lines below. Nothing else.
#include "Include/game/UnknownHomes_Game.h"
#include "RioModPack/ModOptions.h"

// ---- the options UI itself, never gated -- it is how you reach the toggles --
#include "RioModPack/Options Menu.c"
#include "Gecko Codes/Global/Boot To Main Menu.c"

// Duplicate Characters gates ITSELF rather than being wrapped in
// CGECKO_GATE_ADDR: it patches game code, so it has to keep running while the
// option is OFF in order to put the original instructions back. A code that is
// gated away entirely can never undo itself.
//
// CGECKO_OPTION_ADDR is set anyway. It does not gate anything -- it only tells
// cgecko which option this mod's .notes describe, so the Options menu row for
// MODOPT_DUPLICATES can look them up. Without it the notes would be ini-only,
// because the notes table keys on the gate and this mod has none.
#undef  CGECKO_OPTION_ADDR
#define CGECKO_OPTION_ADDR MODOPT_ADDR(MODOPT_DUPLICATES)
#undef  CGECKO_ACTIVE
#define CGECKO_ACTIVE ModOptionOn(MODOPT_DUPLICATES)
#include "Gecko Codes/Menu/Duplicate Characters.c"
#undef  CGECKO_ACTIVE
#define CGECKO_ACTIVE 1
#undef  CGECKO_OPTION_ADDR
#define CGECKO_OPTION_ADDR CGECKO_GATE_ADDR

// Gecko Codes self-gates for the same reason Duplicate Characters does: it
// patches the emulator's code handler, so it has to keep running while the
// option is ON in order to put the handler's entry instruction back. It is
// also the one option that is ON by default -- see ModOptions_ApplyDefaults --
// because "off" here means switching something OFF that a user has enabled
// outside this build, and doing that uninvited would be wrong.
#undef  CGECKO_OPTION_ADDR
#define CGECKO_OPTION_ADDR MODOPT_ADDR(MODOPT_GECKO)
#include "RioModPack/Gecko Codes.c"
#undef  CGECKO_OPTION_ADDR
#define CGECKO_OPTION_ADDR CGECKO_GATE_ADDR

// ---- toggleable mods -------------------------------------------------------
#undef  CGECKO_GATE_ADDR
#define CGECKO_GATE_ADDR MODOPT_ADDR(MODOPT_WIDESCREEN)
#include "Gecko Codes/Global/Widescreen.c"

#undef  CGECKO_GATE_ADDR
#define CGECKO_GATE_ADDR MODOPT_ADDR(MODOPT_NIGHT_MARIO)
#include "Gecko Codes/Menu/Nighttime Mario Stadium.c"

// Custom music is configured, not switched: its eight slots each hold a track,
// and "off" is every slot on Default. So it is NOT gated -- gating it away
// would also stop it handing the audio back when a match loads. Only the notes
// key is set, so the Options row for MODOPT_MUSIC can find its description.
#undef  CGECKO_GATE_ADDR
#define CGECKO_GATE_ADDR 0
#undef  CGECKO_OPTION_ADDR
#define CGECKO_OPTION_ADDR MODOPT_ADDR(MODOPT_MUSIC)
#include "RioModPack/Custom Music.c"

// The Dictionary theme is one of the tracks the menu slot can be set to, and it
// is the only one that is not a stream -- it is a Musyx FX layer. Rather than
// reimplement that swap inside Custom Music, the standalone mod that already
// does it is included here and told when to be active. CGECKO_ACTIVE is the
// condition it self-gates on; on its own the file defaults it to 1 and behaves
// exactly as it always has.
//
// The mod tests CGecko's CGECKO_ACTIVE, so the pack defines that to the
// condition before including the file -- the mod itself is unchanged from its
// standalone form.
//
// NOT wrapped in CGECKO_GATE_ADDR: it edits game state (fx 484's layer id and
// the menu music volume), so it has to keep running while off in order to put
// those back -- the same reason Duplicate Characters self-gates.
// DMM_SCREENS adds the Options screen (6) to the main menu (5). Without it the
// swap does not happen until the user backs out, while every other track in the
// music config swaps the moment it is selected -- so Dictionary looks like the
// one option that does not work.
#undef  CGECKO_ACTIVE
#define CGECKO_ACTIVE (MusicSlot(MUSIC_SLOT_MENU) == MUSIC_DICTIONARY)
#define DMM_SCREENS(sc) ((sc) == 5 || (sc) == 6)
#include "Gecko Codes/Menu/Dictionary Replaces Menu Music.c"
#undef  CGECKO_ACTIVE
#define CGECKO_ACTIVE 1

#undef  CGECKO_OPTION_ADDR
#define CGECKO_OPTION_ADDR CGECKO_GATE_ADDR

#undef  CGECKO_GATE_ADDR
#define CGECKO_GATE_ADDR 0
