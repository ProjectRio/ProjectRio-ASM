###########################################################
# Instant Random Teams
###########################################################
# Author: LittleCoaks, Roeming

# 0x80750c48 --> Fielding Position (team0)
# 0x80750c4c --> Fielding Position (team1)
# 0x80750c10 --> Character ID (team0)
# 0x80750c14 --> Character ID (team1)
# 0x8064d040 --> addCharToTeam function
#       - param r3 is team id
#       - checks char id addr and adds to fielding pos addr roster spot (0xa == random)

# 0x80750C03 --> Captain ID (team0)
# 0x80750C07 --> Captain ID (team1)
# 0x806546a0 --> addCaptainToTeam function
#       - param is team id
#       - checks captain id addr

# 0x803530f7 --> character wasPicked bool array (l == 54)
# 0x800fe5d4 --> allowed captains array (l == 12)

###########################################################
###########################################################
# Grab Active Ports


# Inject: 0x806523a0
Start:
  mflr r0                   # backup registers
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)

  li r0, 1
  stb r0, 0x10(r30)         # sets opponent to a human player

Select_Captains:
  li r30, 0
  lis r29, 0x8075
  ori r29, r29, 0x0C03

Random_Num:
  mflr r0                   # backup registers
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)
  mr r3, r30
  li r4, 0xB                # r4 == range inclusive
  lis r9, 0x8004
  ori r9, r9, 0x2bf0
  mtctr r9
  bctrl
  stb r3, 0(r29)
  lmw r3,0x8(r1)            # restore registers
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0
  addi r30, r30, 1
  addi r29, r29, 4
  cmpwi r30, 2
  bne Random_Num
  li r30, 0

Store_Captains:
  mflr r0                   # backup registers
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)
  mr r3, r30
  lis r4, 0x8065
  ori r4, r4, 0x46a0
  mtctr r4
  bctrl
  lmw r3,0x8(r1)            # restore registers
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0
  addi r30, r30, 1
  cmpwi r30, 2
  bne Store_Captains

End:
  lmw r3,0x8(r1)            # restore registers
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0
  lbz	r0, 0x10 (r30)







###########################################



# Inject: 0x806416cc
START:
  mflr r0                   # backup registers
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)

SET_SCENE_CAPT:
  lis r4, 0x8065
  ori r4, r4, 0x8050
  mtctr r4
  bctrl

SELECT_CAPTAINS:
  li r30, 0
  lis r29, 0x8075
  ori r29, r29, 0x0C03

RANDOM_NUM:
  mflr r0                   # backup registers
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)
  mr r3, r30
  li r4, 0xB                # r4 == range inclusive
  lis r9, 0x8004
  ori r9, r9, 0x2bf0
  mtctr r9
  bctrl
  stb r3, 0(r29)
  lmw r3,0x8(r1)            # restore registers
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0
  addi r30, r30, 1
  addi r29, r29, 4
  cmpwi r30, 2
  bne RANDOM_NUM
  li r30, 0

STORE_CAPTAINS:
  mflr r0                   # backup registers
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)
  mr r3, r30
  lis r4, 0x8065
  ori r4, r4, 0x46a0
  mtctr r4
  bctrl
  lmw r3,0x8(r1)            # restore registers
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0
  addi r30, r30, 1
  cmpwi r30, 2
  bne STORE_CAPTAINS

CONFIRM_CAPTAINS:
  lis r4, 0x8065            # confirm captains
  ori r4, r4, 0x8fc0
  mtctr r4
  bctrl

MAKE_TEAMS:
  li r30, 0                 # team id
  li r4, 0xa                # roster num TODO: use 18 random nums instead of the random arg so color variants can be chosen
  lis r5, 0x8075
  ori r5, r5, 0x0c48
  stb r4, 3(r5)
  stb r4, 7(r5)

RANDOM_TEAMS:
  mflr r0                   # backup registers
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)
  mr r3, r30
  lis r9, 0x8064
  ori r9, r9, 0xd040
  mtctr r9
  bctrl
  lmw r3,0x8(r1)            # restore registers
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0
  addi r30, r30, 1
  cmpwi r30, 2              # call for both teams
  bne RANDOM_TEAMS

CONFIRM_TEAMS:
  li r4, 0
  lis r5, 0x8075
  ori r5, r5, 0x0c70
  stb r4, 0(r5)
  stb r4, 1(r5)
  stb r4, 2(r5)
  stb r4, 3(r5)
  stb r4, 4(r5)
  lis r4, 0x8035
  ori r4, r4, 0x3251
  li r3, 1
  stb r3, 0(r4)
  lis r9, 0x8064            # confirm team func (a)
  ori r9, r9, 0x79d4
  mtctr r9
  bctrl
  lis r9, 0x8064            # confirm team func (b)
  ori r9, r9, 0x9f7c
  mtctr r9
  bctrl
  li r3, 0                  # confirm team 0
  lis r9, 0x8006
  ori r9, r9, 0x5dec
  mtctr r9
  bctrl
  li r3, 1                  # confirm team 1
  lis r9, 0x8006
  ori r9, r9, 0x5dec
  mtctr r9
  bctrl

END:
  lmw r3,0x8(r1)            # restore registers
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0
  li r3, 11





'''
SELECT_CAPTAINS:
  lis r3, 0x800e            # init pointers
  ori r3, r3, 0x870c
  li r4, 1
  stb r4, 0(r3)
  lis r3, 0x8037
  ori r3, r3, 0x1190
  lis r4, 0x8036
  ori r4, r4, 0xf14c
  lis r5, 0x8036
  ori r5, r5, 0xf3c8
  lis r6, 0x80fd
  ori r6, r6, 0x9020
  stw r4, 0x8(r3)
  stw r5, 0xc(r3)
  stw r6, 0x34(r3)

  li r3, 0                  # actually confirm team 0 
  lis r4, 0x8006
  ori r4, r4, 0x5dec
  mtctr r4
  bctrl
  li r3, 1                  # actually confirm team 1
  lis r4, 0x8006
  ori r4, r4, 0x5dec
  mtctr r4
  bctrl
'''





