###########################################################
# Manual Fielder Select v4.0
###########################################################
# Authors: PeacockSlayer, LittleCoaks



###########################################################
###########################################################

# Requires: 20678F8C 88061BD1
# Inject: 0x80678F8C
START:
  mr r14, r3                  # move arg to allow backup without lost info
  mflr r0                     # backup stack frame
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)

# we store the previous manual select arg here; set r9 to the addr
GET_OLD_SELECT:
  lis r9, 0x802E
  ori r9, r9, 0xBF97

CHECK_UNFIELDED:
  lis r4, 0x8089
  ori r4, r4, 0x2701
  lbz r4, 0(r4)
  cmpwi r4, 0                # make sure BallState is unfielded
  bne DESELECT

# store raw button inputs to r8
GET_FIELDER_INPUTS:
  lis r8, 0x8089
  ori r8, r8, 0x2898
  lhz r8, 0(r8)

# convert button input into arg and store in r4
# 0x0: Nothing
# 0x1: Pitcher
# 0x2: Catcher
# 0x3: Closest fielder
GET_SELECT_ARG:
  lbz r4, 0(r9)

CheckButton_Y:
  andi. r5, r8, 0x800
  cmpwi r5, 0x800
  bne CheckButton_X
  li r4, 0x1
  stb r4, 0(r9)

CheckButton_X:
  andi. r5, r8, 0x400
  cmpwi r5, 0x400
  bne CheckButton_R
  li r4, 0x2
  stb r4, 0(r9)

CheckButton_R:
  andi. r5, r8, 0x20
  cmpwi r5, 0x20
  bne CheckValidArgs
  li r4, 0x3
  stb r4, 0(r9)

# li r20, 0xFF                # Set r20 to 0xFF. Means no one is selected. Will set later if someone is ?
CheckValidArgs:
  cmpwi r4, 0x4               # Not a valid input for character ctrl status update. Clear button state
  bge DESELECT
  cmpwi r4, 0x0               # Not a valid input for character ctrl status update. Clear button state
  beq DESELECT

ARG_LOGIC:
  cmpwi r4, 3
  beq SELECT_CLOSEST
  subi r4, r4, 1
  b AssignFielder             # r4 is the fielder position to be selected; pitcher & catcher arg are already set

# 3.
SELECT_CLOSEST:
  li r5, 0x0                   # r5 = for loop increment
  li r4, 0x0                   # r4 = min distance fielder
  lis r6, 0x8088               # r6 = P position addr (ba = 0x8088F368)
  ori r6, r6, 0xF368
  lis r7, 0x8089               # r7 = Ball position addr (ba = 0x80890B38)
  ori r7, r7, 0xB38

Compute_Closest:
  lfs f28, 0(r7)
  lfs f29, 0(r6)
  fsub f30, f28, f29
  fadd f30, f30, f30
  fabs f30, f30
  lfs f28, 8(r7)
  lfs f29, 8(r6)
  fsub f28, f28, f29
  fadd f28, f28, f28
  fabs f28, f28
  fadd f30, f30, f28             # f30 = current min distance
  cmpwi r5, 0x0
  beq- StoreClosest
  fcmpo cr0, f10, f30
  blt- IncrementLoop_Closest

StoreClosest:
  fmr f10, f30                   # f10 = previous min distance
  mr r4, r5

IncrementLoop_Closest:
  addi r5, r5, 0x1
  addi r6, r6, 0x268
  cmpwi r5, 0x9
  bne+ Compute_Closest

  b AssignFielder

# Finally set desired player to 0xF. Reset vars. Return
AssignFielder:
  lis r6, 0x8088
  ori r6, r6, 0xF53B
  li r8, 0                      # for loop increment

FIELDER_LOOP:
  mulli r9, r8, 0x268
  lbzx r5, r9, r6
  cmpwi r5, 0xF
  bne IncrementLoop_Fielder
  li r5, 0xC
  stbx r5, r9, r6

IncrementLoop_Fielder:
  addi r8, r8, 1
  cmpwi r8, 0x9
  blt FIELDER_LOOP
  
  mulli r5, r4, 0x268           # compute offset for fielder addr
  li r7, 0xF                   # set to 0xF (selected)
  stbx r7, r5, r6
  lis r6, 0x8089               # Finally set the hand indicator + lock
  ori r6, r6, 0x2800
  stb r4, 0x1(r6)
  stb r4, 0x7(r6)
  b END_FAKE

DESELECT:
  li r4, 0x0
  stb r4, 0(r9)                 # set previous select arg to 0

END_REAL:
  lmw r3,0x8(r1)                # restore stack
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0
  mr r3, r14
  lbz r0, 0x1bd1(r6)            # Perform op where we inject
  b END

END_FAKE:
  lmw r3,0x8(r1)                # restore stack
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0
  mr r3, r14
  li r0, 1

END:




