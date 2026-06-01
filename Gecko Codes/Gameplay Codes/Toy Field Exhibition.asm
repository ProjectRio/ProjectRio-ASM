###########################################################
# Toy Field Exhibition
###########################################################
# Author: LittleCoaks

# Address: 0x80650674
# State: Menu

###########################################################
###########################################################

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
   
