/*###########################################################
# Nighttime Mario Stadium
###########################################################*/
// Author: LittleCoaks
//
// *Mario Stadium is always the nighttime version in exhibition mode.
//
// The stadium-select screen writes the chosen stadium into the match-setup
// struct at
//
//     0x80650674  stb r5, 9(r4)      stadium id
//     0x80650678  stb r0, 0x58(r3)   <- hook site, r4 still the setup struct
//
// and the byte right after the stadium id, +0xA, is the day/night flag the
// stadium loader reads (0 = day, 1 = night). Mario Stadium is id 0 and the
// only stadium with a night variant, so once the id is written we flip the
// flag for it and leave every other stadium alone.
//
// Fire-and-forget: nothing is patched, so a pack can gate this whole code away
// with CGECKO_GATE_ADDR and the stock write path is simply left untouched.
#include "Include/game/UnknownHomes_Game.h"

#define STADIUM_MARIO   0
#define STADIUM_NIGHT   1

CGECKO(NighttimeMarioStadium, .address = 0x80650678, .state = MSSB_MENU,
       .instruction = "stb r0, 0x58(r3)",
       .notes = "Mario Stadium is played at night\n"
                "in exhibition mode.");
void NighttimeMarioStadium()
{
    READ_GAME_REG(u8*, setup, 4);       // the match-setup struct

    if (setup[9] == STADIUM_MARIO)
        setup[0xA] = STADIUM_NIGHT;
}
