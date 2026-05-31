###########################################################
# Pitche Lag Reduction - Helper
###########################################################
# Author: LittleCoaks

# Address: 0x8069e1d4
# State: Game

###########################################################
###########################################################

Start:
  mflr r0                   # backup registers
  stw r0, 0x4(r1)
  stwu r1, -0x100(r1)
  stmw r3, 0x8(r1)

# r5 = current left or right
# r6 = -1 left or right
Get_Input:
  li r5, 0
  lis r8, 0x8089
  ori r8, r8, 0x2899
  lbz r8, 0(r8)

 Check_Left:
  andi. r5, r8, 0x1
  cmpwi r5, 0x1
  beq Store_Input

Check_Right:
  andi. r5, r8, 0x2

Store_Input:
  lis r7, 0x802E
  ori r7, r7, 0xBF98
  stb r5, 0x0(r7)

End:
  lmw r3,0x8(r1)            # restore registers
  lwz r0, 0x104(r1)
  addi r1,r1, 0x100
  mtlr r0
  lwz	r0, 0x0014 (sp)     # replace instruction
