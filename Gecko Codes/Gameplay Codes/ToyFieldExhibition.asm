###########################################################
# Toy Field Exhibition
###########################################################
# Author: LittleCoaks



###########################################################
# Select Toy Field

# Requires: 20650674 98a40009
# Inject: 0x80650674
Start:
  li r15, 0x0                   # init r18 to p1 inputs
  lis r18, 0x802E
  ori r18, r18, 0x9F40

Check_Inputs:
  lbz r19, 1(r18)
  andi. r19, r19, 0xC0
  cmpwi r19, 0xC0
  beq- Set_ToyField
  addi r15, r15, 0x1            # check next port
  cmpwi r15, 0x4
  beq- End
  addi r18, r18, 0x8
  b Check_Inputs

Set_ToyField:
  li r5, 0x6

End:
  stb r5, 9(r4)
  


###########################################################
# Play Sound Effect Queue

# Requires: 20640d54 386001b8
# Inject: 0x80640d54
Start:
  li r3, 0x1b8
  lis r5, 0x800E
  ori r5, r5, 0x8705
  lbz r5, 0(r5)
  lis r6, 0x800e
  ori r6, r6, 0x877e
  lhz r6, 0(r6)
  add r5, r5, r6
  cmpwi r5, 0x12
  bne End
  li r3, 0x1bc

End:
  

