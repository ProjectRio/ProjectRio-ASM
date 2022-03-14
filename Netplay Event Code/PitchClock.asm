###########################################################
# Pitch Clock
###########################################################
# Author: LittleCoaks



###########################################################
###########################################################
# Requires: 206B4070 540005ef

# Writes:
# 046b4490 60000000
# 046b42d0 60000000
# 046b46b8 60000000

# Inject @ 806b4070

START:
  lis r14, 0x8089               # initialize r14 to pitch clock addr
  ori r14, r14, 0x0ae0
  lhz r14, 0(r14)
  cmpwi r14, 0x258              # check if it's equal to 10 seconds (600 frames)
  bne END
  li r0, 0x0100                 # force fielder to press A

END:
  rlwinm.	r0, r0, 0, 23, 23
c

