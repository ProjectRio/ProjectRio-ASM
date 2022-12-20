###########################################################
# Highlight Ball Shadow
###########################################################
# Author: LittleCoaks



###########################################################
###########################################################
# Requires: 206a844c 41820224

# Inject: 0x806a844c
Start:
  bne End                 # previous instruction checked if r4 was 0 (drop spots off)l 
  lis r4, 0x806a
  ori r4, r4, 0x85b8      # ba for instructions to overwrite
  lis r12, 0xc01d         # value to change instruction to
  stw r12, 0(r4)
  lis r12, 0xc005
  ori r12, r12, 0x8
  stw r12, 0x1c(r4)

End:
  