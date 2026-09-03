/*###########################################################
# CPU vs CPU
###########################################################*/
// Author: LittleCoaks

// Known bug: CPU never charge swings?

#include "Include/game/UnknownHomes_Game.h"

// no .address -> runs once per frame, while the game rel is resident
CGECKO(CPUvsCPU, .state = MSSB_GAME);
void CPUvsCPU()
{
    g_GameLogic.AIDifficulty0Special3Weak[0] = 0;
    g_GameLogic.AIDifficulty0Special3Weak[1] = 0;
    
    g_GameLogic.teamAIInd[0] = 1;
    g_GameLogic.teamAIInd[1] = 1;
    g_GameLogic.runnerAIInd[0] = 1;
    g_GameLogic.runnerAIInd[1] = 1;
    g_GameLogic.battingAIInd[0] = 1;
    g_GameLogic.battingAIInd[1] = 1;
    
    g_GameLogic.teamIsCPU[0] = 1;
    g_GameLogic.teamIsCPU[1] = 1;
    g_GameLogic.autoFielding[0] = 1;
    g_GameLogic.autoFielding[1] = 1;
    
    g_Pitcher.AIInd = 1;
    g_Pitcher.aiLevel = 0;
    g_Batter.aiControlledInd = 1;
    g_Batter.aiLevel= 0;
}