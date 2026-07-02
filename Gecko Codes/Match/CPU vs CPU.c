/*###########################################################
# CPU vs CPU
###########################################################*/
// Author: LittleCoaks
// State: Game


#include "Game Data/GameData.h"
#define controllerPorts_ADDR VAR_ADDRESS(int, 2, 0x80892A78)

void CPUvsCPU()
{
    gameControls.AIDifficulty0Special3Weak[0] = 0;
    gameControls.AIDifficulty0Special3Weak[1] = 0;
    gameControls.runnerAIInd_[0] = 1;
    gameControls.runnerAIInd_[1] = 1;
    gameControls.teamIsCPU[0] = 1;
    gameControls.teamIsCPU[1] = 1;
    gameControls.teamAIInd[0] = 1;
    gameControls.teamAIInd[1] = 1;
    gameControls.autoFielding[0] = 1;
    gameControls.autoFielding[1] = 1;
    inMemPitcher.AIInd_ = 1;
    inMemBatter.aIControlledInd = 1;
}