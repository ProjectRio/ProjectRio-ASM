###########################################################
# Anti Quick Pitch
###########################################################
# Author: PeacockSlayer, LittleCoaks



###########################################################
###########################################################
# Requires: "206B406C a01e0006"

# Inject: 0x806B406C
START:
  lis r14, 0x8089                   # Check if we're playing a normal game
  ori r14, r14, 0x80DE
  lbz r14, 0(r14)
  cmpwi r14, 0x0
  bne- GET_INPUT_NORMALLY           # 0 == exhibition/toy field; else == minigames

SHOULD_SKIP_INPUT:
  li r0, 0x0                        # Check if batter is in lag
  lis r14, 0x8089
  ori r14, r14, 0x99D
  lbz r14, 0(r14)
  cmpwi r14, 0x1
  beq- END

GET_INPUT_NORMALLY:
  lhz r0, 6(r30)

END:

