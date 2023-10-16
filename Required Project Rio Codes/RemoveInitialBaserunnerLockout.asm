###########################################################
# Remove Initial Baserunner Lockout
###########################################################
# Authors: nuche17, LittleCoaks



###########################################################
###########################################################

# Inject: 0x806c9d78
Start:
  lis r14, 0x800F
  subi r14, r14, 0x7884
  lha r14, 0(r14)
  cmpwi r14, 5
  beq RunMod
  mflr r0              # main menu instruction at this addr
  b End
RunMod:
  lha r0, 0x6 (r29)
  lis r14, 0x8089
  ori r14, r14, 0x2701
  lbz r14, 0(r14)
  cmpwi r14, 0
  bne End
  li r0, 1

End:
