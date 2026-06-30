/*###########################################################
# 4 Player Match
###########################################################*/
// Author: LittleCoaks
// State: Game
// *Batting: swaps every at bat
// *Fielding: swaps every inning. One teammate is pitcher while the other fields

#include "Game Data/GameData.h"

#define controllerPorts_ADDR VAR_ADDRESS(int, 2, 0x80892A78)
#define autogolf_ports VAR_ADDRESS(byte, 2, 0x802EBF94) // [0] = fielder, [1] = batter

void FourPlayerMatch()
{
    int original_fielder_port = controllerPorts_ADDR[gameControls.fieldingTeam_P1_P2_];
    int original_batter_port = controllerPorts_ADDR[gameControls.battingTeam_P1_P2_];

    // don't run if game hasn't started
    if (!gameControls.EventTriggers_GameHasStarted_)
        return;

    // i = how much to increment port by to get the other player on the same team
    // PlayerPorts = the original ports selected on the menu
    int i_port = PlayerPorts[1] == 1 ? 2 : 1;

    int fielding_team_player_a = PlayerPorts[0];
    int fielding_team_player_b = PlayerPorts[0] + i_port;
    int batting_team_player_a = PlayerPorts[1];
    int batting_team_player_b = PlayerPorts[1] + i_port;

    bool is_odd_inning = GameState.Inning % 2 == 1;
    bool is_at_bat = (gameControls.sceneID == enumSceneID_atBat) || (gameControls.sceneID == enumSceneID_replay_atBat);

    // fielding -- one teammate pitches while the other fields the whole inning; alternate each inning
    bool use_fielder_a = (is_odd_inning && is_at_bat) || (!is_odd_inning && !is_at_bat);
    int fielder_port = use_fielder_a ? fielding_team_player_a : fielding_team_player_b;
    controllerPorts_ADDR[gameControls.fieldingTeam_P1_P2_] = fielder_port;
    autogolf_ports[0] = fielder_port + 1;

    // batting -- teammates alternate each at-bat
    int number_ABs = 0;
    for (int i = 0; i <= 8; i++)
    {
        number_ABs += BatterStats_P1_P2_[gameControls.battingTeam_P1_P2_][i].AtBats;
    }

    bool use_batter_a = number_ABs % 2 != 0;
    int batter_port = use_batter_a ? batting_team_player_a : batting_team_player_b;
    controllerPorts_ADDR[gameControls.battingTeam_P1_P2_] = batter_port;
    autogolf_ports[1] = batter_port + 1;

    if (original_fielder_port != fielder_port)
        OSReport("Fielder Port: %d\n", fielder_port + 1);

    if (original_batter_port != batter_port)
    {
        OSReport("Batter Port: %d\n", batter_port + 1);
        OSReport("nABs: %d\n", storedInningInfo.nABs_[gameControls.battingTeam_P1_P2_]);
    }
}