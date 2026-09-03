/*###########################################################
# Teams Exhibition 2
###########################################################*/
// Author: LittleCoaks

// *Allows 1v2, 2v2, and 1v3 team matches in exhibition mode
// *Batting:  swaps every plate appearance
// *Fielding: swaps every inning (2 players) or every batter (3 players)
// *
// *Game type is based on who drafts:
// *  Port 2 -> 1v2 (P1 vs P2/P3)
// *  Port 3 -> 2v2 (P1/P2 vs P3/P4)
// *  Port 4 -> 1v3 (P1 vs P2/P3/P4)

#include "Include/game/UnknownHomes_Game.h"

#include "Include/static/UnknownHomes_Static.h"
// TEMPORARY: consumed by current Rio client versions for autogolf mode.
// Remove once the client no longer reads 0x802EBF94/5.
#define autogolf_ports ARRAY_1D_ADDRESS(u8, 2, 0x802EBF94)

// Rotation logic shared by every section below (always inlined into each
// entry function). Computes which physical ports (0-3) should have control
// right now; returns false when this isn't a supported teams game.
static inline __attribute__((always_inline))
bool GetActivePorts(int* fielder_port_out, int* batter_port_out)
{
    int second_drafter = g_d_GameSettings.PlayerPorts[1];

    // 1v2 (1), 2v2 (2), and 1v3 (3) are supported; vs-CPU / anything else -> inactive
    if (second_drafter < 1 || second_drafter > 3)
        return false;

    // team rosters as (base port, size). Team 0 starts at port 0; team 1 fills the
    // ports immediately after it, so its base is team 0's size (NOT PlayerPorts[1]).
    int team_base[2];
    int team_size[2];
    team_base[0] = 0;
    team_size[0] = (second_drafter == 2) ? 2 : 1;   // 2v2 -> team 0 has two; else solo
    team_base[1] = team_size[0];
    team_size[1] = (second_drafter == 3) ? 3 : 2;   // 1v3 -> team 1 has three; else two

    int field_team = g_GameLogic.teamFielding;
    int bat_team   = g_GameLogic.teamBatting;

    // Plate appearances by the batting team. This doubles as the count of batters
    // the fielding team has faced, so it drives both the batter round-robin and the
    // 3-player pitcher/fielder rotation. plateAppearances (unlike AtBats) counts walks.
    int number_PAs = 0;
    for (int i = 0; i <= 8; i++)
    {
        number_PAs += Static_Stats_Tables.batterStats[bat_team][i].plateAppearances;
    }

    // replay_atBat counts as at-bat for the pitcher/fielder role: TeamsExhibition
    // never runs during replays, but the HUD icon hooks can.
    int scene = g_GameLogic.sceneID;
    bool is_at_bat = (scene == SCENE_ID_AT_BAT) || (scene == SCENE_ID_REPLAY_AT_BAT);

    // fielding
    int field_base = team_base[field_team];
    int field_size = team_size[field_team];
    int fielder_port;
    if (field_size == 1)
    {
        // solo defender pitches AND fields
        fielder_port = field_base;
    }
    else if (field_size == 2)
    {
        // two defenders split pitcher/fielder and trade roles each inning
        bool is_odd_inning = g_Scores.Inning % 2 == 1;
        bool use_fielder_a = (is_odd_inning == is_at_bat);
        fielder_port = use_fielder_a ? field_base : field_base + 1;
    }
    else
    {
        // three defenders: rotate the pitcher/fielder pair each batter faced; the
        // third rests. Each player cycles pitch -> field -> rest across three batters.
        int r = number_PAs % 3;
        int pitcher_port = field_base + r;
        int fielder2_port = field_base + (r + 1) % 3;
        fielder_port = is_at_bat ? pitcher_port : fielder2_port;
    }
    *fielder_port_out = fielder_port;

    // batting -- round-robin through the batting team's players each plate appearance
    // (size 1 -> always the one player; size 2 -> A/B toggle; size 3 -> P2/P3/P4)
    *batter_port_out = team_base[bat_team] + (number_PAs % team_size[bat_team]);

    return true;
}

/*-----------------------------------------------------------
 Section 1: hand over control (end of UpdateControllerInputs)

 Both codes here are .state = MSSB_GAME: every address is game
 inningSetting.rel code, only valid while inningSetting.rel == 5.
-----------------------------------------------------------*/

CGECKO(TeamsExhibition, .address = 0x806AC530, .state = MSSB_GAME,
                        .instruction = "blr");
void TeamsExhibition()
{
    // Freeze during replays: playback feeds the recorded inputs back through
    // the (unmodified) teamPorts mapping; copying live inputs over them
    // would corrupt the replay.
    int scene = g_GameLogic.sceneID;
    if (scene == SCENE_ID_REPLAY_AT_BAT || scene == SCENE_ID_REPLAY_LIVE_BALL)
        return;

    int fielder_port, batter_port;
    if (!GetActivePorts(&fielder_port, &batter_port))
        return;

    // TEMPORARY: report the active ports to the Rio client for autogolf mode
    autogolf_ports[0] = fielder_port + 1;
    autogolf_ports[1] = batter_port + 1;

    // hand control over by overwriting the read port's inputs with the active
    // teammate's. The game reads team 0 from port 0 and team 1 from the
    // drafting port (the untouched teamPorts values). Whole-struct copy keeps
    // newInputOnLatestFrame edges coherent since they were computed against
    // the source port's own history.
    int read_port[2];
    read_port[0] = 0;
    read_port[1] = g_d_GameSettings.PlayerPorts[1];

    int field_team = g_GameLogic.teamFielding;
    int bat_team   = g_GameLogic.teamBatting;
    int active_port[2];
    active_port[field_team] = fielder_port;
    active_port[bat_team]   = batter_port;

    for (int t = 0; t < 2; t++)
    {
        int src = active_port[t];
        int dst = read_port[t];
        if (src == dst)
            continue;
        // InputStruct is 0x10 bytes; copy as 4 words to avoid a memcpy call
        u32* s = (u32*)&g_Controls[src];
        u32* d = (u32*)&g_Controls[dst];
        d[0] = s[0];
        d[1] = s[1];
        d[2] = s[2];
        d[3] = s[3];
    }
}

/*-----------------------------------------------------------
 Section 2: HUD port icons (draw_ongoingStarGuageHud)

 draw_initStarGuageHud picks each team's port icon from
 g_GameLogic.teams when the HUD is created and never
 updates it, so the icon goes stale as the rotation advances.
 This hook runs every frame the HUD is drawn and re-writes the
 icon frame u32 the same way init does:
   manager   = *(0x803CC1B8); slot index = u16 at manager+0x14
   entry     = 0x80371C30 + index*8   (graphics object pair table)
   objects   = *(entry+0x78) fielding icon, *(entry+0x80) batting icon
   icon u32 = object+0x5C, value port << 16
                (frames 0-3 = human 1P-4P; +4 = the CPU variants)
 Injected at an idle `li r5, -1` inside the ongoing draw so the
 objects are guaranteed to exist.
-----------------------------------------------------------*/

#define hud_manager    VAR_ADDRESS(u8*, 0x803CC1B8)
#define hud_slot_table 0x80371C30

CGECKO(TeamsPortIcons_Ongoing, .address = 0x806D8EC0, .state = MSSB_GAME,
                               .instruction = "li r5, -1");
void TeamsPortIcons_Ongoing()
{
    int fielder_port, batter_port;
    if (!GetActivePorts(&fielder_port, &batter_port))
        return;

    u8* mgr = hud_manager;
    if (!mgr)
        return;
    int idx = *(u16*)(mgr + 0x14);
    u8* entry = (u8*)(hud_slot_table + (idx << 3));

    u8* field_obj = *(u8**)(entry + 0x78);
    u8* bat_obj   = *(u8**)(entry + 0x80);
    if (field_obj)
        *(u32*)(field_obj + 0x5C) = (u32)fielder_port << 16;
    if (bat_obj)
        *(u32*)(bat_obj + 0x5C)   = (u32)batter_port << 16;
}
