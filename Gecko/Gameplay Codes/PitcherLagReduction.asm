###########################################################
# Pitche Lag Reduction
###########################################################
# Author: LittleCoaks



###########################################################
###########################################################
# Requires: 2069e1d4 80010014

# Inject: 0x8069e1d4
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




###########################################################
###########################################################
# Requires: 206b05c4 3c608089

# Inject: 0x806b05c4
Start:
  mflr r0                   # backup registers
  stw r0, 0x4(r1)
  stwu r1, -0x100(r1)
  stmw r3, 0x8(r1)
  lis r10, 0x802E
  ori r10, r10, 0xBFB4
  lis r8, 0x8089
  ori r8, r8, 0x0A1C
  lfs r8, 0x0(r8)
  stfs r8, 0x0(r10)

# r5 = current left or right
# r6 = last frame left or right
Get_Input:
  lis r8, 0x8089
  ori r8, r8, 0x3928
  lis r9, 0x8089
  ori r9, r9, 0x298c
  lbz r9, 0xEC(r9)
  rlwinm r6, r6, 4, 0, 27
  add r8, r8, r9
  lbz r8, 0x5(r8)

 Check_Left:
  andi. r5, r8, 0x1
  cmpwi r5, 0x1
  beq Get_Previous_Inputs

Check_Right:
  andi. r5, r8, 0x2

Get_Previous_Inputs:
  lis r6, 0x802E
  ori r6, r6, 0xBF98
  lbz r7, 0x1(r6)
  lbz r6, 0x0(r6)

# recalling the function
P1_and_C:
  cmpw r5, r6
  beq End
  mflr r0                   # backup registers
  stw r0, 0x4(r1)
  stwu r1, -0x100(r1)
  stmw r3, 0x8(r1)

  lis r8, 0x806a
  ori r8, r8, 0xff88
  mtctr r8
  bctrl

  lmw r3,0x8(r1)            # restore registers
  lwz r0, 0x104(r1)
  addi r1,r1, 0x100
  mtlr r0

  lis r8, 0x8089
  ori r8, r8, 0x0A1C
  lfs r8, 0x0(r8)
  lfs r9, 0x0(r10)
  fadd r8, r8, r9
  stfs r8, 0x0(r10)

  mflr r0                   # backup registers
  stw r0, 0x4(r1)
  stwu r1, -0x100(r1)
  stmw r3, 0x8(r1)

  lis r8, 0x806a
  ori r8, r8, 0xff88
  mtctr r8
  bctrl

  lmw r3,0x8(r1)            # restore registers
  lwz r0, 0x104(r1)
  addi r1,r1, 0x100
  mtlr r0

  lis r8, 0x8089
  ori r8, r8, 0x0A1C
  lfs r8, 0x0(r8)
  lfs r9, 0x0(r10)
  fadd r8, r8, r9
  stfs r8, 0x0(r10)

End:
  lis r8, 0x8089
  ori r8, r8, 0x0A1C
  lfs r9, 0x0(r10)
  stfs r9, 0x0(r8)
  lmw r3,0x8(r1)            # restore registers
  lwz r0, 0x104(r1)
  addi r1,r1, 0x100
  mtlr r0
  lis r3, 0x8089            # replace instruction


