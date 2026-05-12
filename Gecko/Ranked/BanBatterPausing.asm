###########################################################
# Restrict Batter Pausing
###########################################################
# Author: LittleCoaks



###########################################################
###########################################################
# Requires: "206EED5C A0040006"

# Inject: 0x806EED5C
Start:
  lis r6, 0x8089                   # Check if batter is in neutral state
  ori r6, r6, 0x99D
  lbz r0, 0(r6)
  lis r6, 0x8089
  ori r6, r6, 0x9aD
  lbz r6, 0(r6)
  add r6, r0, r6
  lhz r0, 0x0006 (r4)               # get batter pause arg
  cmpwi r6, 0x0
  beq- End
  li r0, 0                          # if batter not neutral, set arg to 0; can't pause

End:
  
