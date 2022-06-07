###########################################################
# Remove Dingus Bunting
###########################################################
# Author: LittleCoaks



###########################################################
###########################################################
# Requires: 2069811c 38000001

# Inject: 0x8069811c
START:
  lis r14, 0x8089               # fielder inputs
  ori r14, r14, 0x2899
  lbz r0, 0(r14)
  andi. r0, r0, 0x10            # is fielder pressing Z
  cmpwi r0, 0
  beq END                       # if not, do not move fielders (r0 is already 0 here, so don't need to change it)
  li r0, 1                      # r0 = 1 is what moves in the fielders

END:


