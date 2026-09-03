/*###########################################################
# CPU Always Sprints
###########################################################*/
// Author: LittleCoaks

// WIP, doesn't work yet

#include "Include/game/UnknownHomes_Game.h"

// no .address -> runs once per frame, while the game rel is resident
CGECKO(CPUAlwaysSprints, .state = MSSB_GAME,
       .notes = "CPU runners and the selected CPU fielder\n"
                "always sprint. Human players are\n"
                "unaffected.");
void CPUAlwaysSprints()
{
    // Part 1: Force all active CPU runners and baserunners to sprint
    if (g_Batter.aiControlledInd) {
        for (int r = 0; r < 4; r++) {
            InMemRunnerType* runner = &g_Runners[r];
            if (runner->runnerOnFieldOrOutOrScored != RUNNER_STATUS_ON_FIELD)
                continue;

            runner->runningDirectionDesired    = RUNNER_MOVEMENT_FORWARDS;
            runner->FramesUntilNotSprinting = 6;
            runner->delayBeforeStartingToRun   = 0;
        }
    }

    // Part 2: Force the currently selected CPU fielder to dash
    s16 selFielder = g_FieldingLogic.selectedFielder;
    if (g_Fielders[selFielder].AI_Ind) {
        int fieldingPort = g_GameLogic.teamFielding;
        g_FieldingLogic.fielderDashByPort[fieldingPort][FIELDER_DASH_SPRINTING_STATE] = DASH_STATE_SPRINTING;
    }
}