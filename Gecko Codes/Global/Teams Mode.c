/*###########################################################
# Teams Mode
###########################################################*/
// Author: LittleCoaks
// State: Game

// *Allows 1v2, 2v2, and 1v3 team matches in exhibition mode
// *Batting:  swaps every plate appearance
// *Fielding: swaps every inning (2 players) or every batter (3 players)
// * 
// *Game type is based on who drafts:
// *  Port 2 -> 1v2 (P1 vs P2/P3)
// *  Port 3 -> 2v2 (P1/P2 vs P3/P4)
// *  Port 4 -> 1v3 (P1 vs P2/P3/P4)

#include "Game Data/GameData.h"

#define autogolf_ports VAR_ADDRESS(byte, 2, 0x802EBF94) // [0] = fielder, [1] = batter

void TeamsMode()
{
    int second_drafter = PlayerPorts[1];

    // 1v2 (1), 2v2 (2), and 1v3 (3) are supported; leave vs-CPU / anything else alone
    if (second_drafter < 1 || second_drafter > 3)
        return;

    // team rosters as (base port, size). Team 0 starts at port 0; team 1 fills the
    // ports immediately after it, so its base is team 0's size (NOT PlayerPorts[1]).
    int team_base[2];
    int team_size[2];
    team_base[0] = 0;
    team_size[0] = (second_drafter == 2) ? 2 : 1;   // 2v2 -> team 0 has two; else solo
    team_base[1] = team_size[0];
    team_size[1] = (second_drafter == 3) ? 3 : 2;   // 1v3 -> team 1 has three; else two

    int field_team = gameControls.fieldingTeam_P1_P2_;
    int bat_team   = gameControls.battingTeam_P1_P2_;

    int field_base = team_base[field_team];
    int field_size = team_size[field_team];
    int bat_base   = team_base[bat_team];
    int bat_size   = team_size[bat_team];

    // Plate appearances by the batting team. This doubles as the count of batters
    // the fielding team has faced, so it drives both the batter round-robin and the
    // 3-player pitcher/fielder rotation. plateAppearances (unlike AtBats) counts walks.
    int number_PAs = 0;
    for (int i = 0; i <= 8; i++)
    {
        number_PAs += BatterStats_P1_P2_[bat_team][i].plateAppearances;
    }

    bool is_at_bat = (gameControls.sceneID == enumSceneID_atBat) || (gameControls.sceneID == enumSceneID_replay_atBat);

    // fielding
    int fielder_port;
    if (field_size == 1)
    {
        // solo defender pitches AND fields
        fielder_port = field_base;
    }
    else if (field_size == 2)
    {
        // two defenders split pitcher/fielder and trade roles each inning
        bool is_odd_inning = GameState.Inning % 2 == 1;
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
    gameControls.teams[field_team] = fielder_port;
    autogolf_ports[0] = fielder_port + 1;

    // batting -- round-robin through the batting team's players each plate appearance
    // (size 1 -> always the one player; size 2 -> A/B toggle; size 3 -> P2/P3/P4)
    int batter_port = bat_base + (number_PAs % bat_size);
    gameControls.teams[bat_team] = batter_port;
    autogolf_ports[1] = batter_port + 1;
}