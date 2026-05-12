###########################################################
# Manual Fielder Select v5.0
###########################################################
# Authors: PeacockSlayer, LittleCoaks



###########################################################
###########################################################

# MECHANICS:
# Uses the vanilla mario baseball control lockouts
# R - closest fielder without hand
#   - to drop spot when ball in the air
#   - to the ball when the ball is grounded
#   - doesn't work when star swing is active & airborne
# Z - undo manual fielder select

# TODO:
#   - currently only swaps to 2nd closest if R is held down. need to fix

# r15 = Mfs vars - 0x0 (previous state), 0x1 (current state), 0x3 (mfs fielder)
# r16 = Frames after contact
# r17 = Number of outs
# r18 = BallState
# r19 = Raw button inputs
# r20 = BallHitState
# r21 = Drop Spot coords
# r22 = Ball coords
# r23 = Fielder lockout array (bytes)
# r24 = Type of Swing
# r25 = Is Star Swing
# r26 = Hand indicator

# Requires: 20678F8C 88061BD1
# Inject: 0x80678F8C
Start:
  mr r14, r3                  # move arg to allow backup without lost info (pointer)
  stwu r1,-0x80(r1)           # backup registers 3-31
  stmw r3,0x8(r1)

# r15 = manual select addresses 0x802EBF96
GetOldSelect:
  lis r15, 0x802E
  ori r15, r15, 0xBF96

# Here's the flow of things here:
# - if it's only the 4th frame after contact, deselect; we do this to prevent manual selects from the previous AB from carrying over
# - else if it's after the 3rd out, allow a manual select to permit moonwalking
# - else if the ball is not in the unfielded state, deselect so that the code doesn't run after fielding the ball
# - else, run the rest of the function

# r16 = frames after contact
# r17 = number of outs
# r18 = BallState
CheckGameState:
  lis r16, 0x8089             # num frames after contact
  ori r16, r16, 0x269e
  lhz r4, 0(r16)
  cmpwi r4, 4
  beq ResetVars                # deselect if 4th frame; this would be the first frame that this code runs
  lis r17, 0x8089
  ori r17, r17, 0x2973
  lbz r4, 0(r17)
  cmpwi r4, 3                 # allow manual select after 3 outs to allow for moonwalking
  bge CheckBallState          # they pay me to make these mods so i gotta deliver i guess
  lis r18, 0x8089
  ori r18, r18, 0x2701
  lbz r4, 0(r18)
  cmpwi r4, 0                 # make sure BallState is unfielded
  beq CheckBallState          # if the ball has been fielded already, set the previous manual select value to 0 (no selection) and end function; no manual selects after fielding ball
  cmpwi r4, 3                 # make suer BallState is unfielded
  beq CheckBallState          # if the ball has been fielded already, set the previous manual select value to 0 (no selection) and end function; no manual selects after fielding ball

ResetVars:
  li r4, 0
  stb r4, 0(r15)
  stb r4, 1(r15)
  li r4, 0xff
  stb r4, 3(r15)
  b EndReal

# r20 = BallHitState
# r21 = Drop Spot coords
# r22 = Ball coords
# r23 = Fielder lockout array (bytes)
# r24 = Type of Swing
# r25 = is Star Swing
CheckBallState:
  lis r20, 0x8089
  ori r20, r20, 0x26B2        # BallHitState
  lis r21, 0x8089             # Drop spot position addr (ba = 0x80890E80)
  ori r21, r21, 0x0E80
  lis r22, 0x8089             # Ball position addr (ba = 0x80890B38)
  ori r22, r22, 0x0B38
  lis r23, 0x807b             # Fielder lockout array (ba = 0x807b5cf8)
  ori r23, r23, 0x5cf8
  lis r24, 0x8089             # Type of Swing
  ori r24, r24, 0x099B
  lis r25, 0x8089             # is Star Swing
  ori r25, r25, 0x09b1
  lhz r7, 0(r20)
  lbz r8, 0(r25)
  cmpwi r8, 0
  beq GetFielderInputs        # if not a star swing, proceed normally
  cmpwi r7, 0                 # if a star swing and ball hasn't landed, deselect
  beq Deselect

# r19 = raw button inputs
GetFielderInputs:
  lis r19, 0x8089
  ori r19, r19, 0x2898
  lhz r6, 0(r19)  

# convert button input into manual select state and store in r4
# 0x0: Nothing
# 0x1: R press
# 0x2: Z press
GetSelectState:
  li r7, 0

CheckButton_Z:
  andi. r5, r6, 0x10
  cmpwi r5, 0x10
  bne CheckButton_R
  li r7, 0x2
  stb r7, 1(r15)               # store our current state to previous state addr

CheckButton_R:
  andi. r5, r6, 0x20
  cmpwi r5, 0x20
  bne StoreHeldInput
  li r7, 0x1
  stb r7, 1(r15)               # store our current state to previous state addr

StoreHeldInput:
  li r5, 0
  lbz r6, 0(r15)
  cmpw r6, r7
  bne 0x8
  li r5, 1
  stb r7, 0(r15)

CheckValidArgs:
  lbz r4, 1(r15)              # get the current state
  cmpwi r4, 0
  beq Deselect                # no MFS yet this play
  cmpwi r5, 1
  beq EndFake                 # user has held the same button. don't assign a new fielder
  cmpwi r4, 0x2
  beq Deselect


# from this point on in the function, the player has either just input a manual select, or did so previously this play
SelectClosest_DropSpot:
  lhz r7, 0(r20)
  cmpwi r7, 0x1               # landed state
  beq SelectClosest_Ball      # select closest to ball if L is pressed but ball has landed
  mr r7, r21
  li r8, 0x4                  # offset distance to Z position; we use this in the ComputeClosest algorithm
  b SelectClosest

SelectClosest_Ball:
  mr r7, r22                  # r7 is the address of the coordinate we're checking
  li r8, 0x8                  # offset distance to Z position; we use this in the ComputeClosest algorithm

SelectClosest:
  lis r26, 0x8089             # hand indicator (0x1)
  ori r26, r26, 0x2800
  li r3, 0x0                  # return register of CanFielderBeAssigned
  li r4, 0xff                 # r4 = min distance fielder
  li r5, 0x0                  # r5 = for loop increment
  lis r6, 0x8088              # r6 = Player position addr (ba = 0x8088F368)
  ori r6, r6, 0xF368
  li r9, 0x0                  # has any fielder been assigned
  lhz r11, 0(r26)

# distance^2 == (x1 - x2)^2 + (y1 - y2)^2
ComputeClosest:
  lfs f28, 0(r7)
  lfs f29, 0(r6)
  fsub f30, f28, f29
  fadd f30, f30, f30
  fabs f30, f30                 # (x1 - x2)^2
  lfsx f28, r8, r7
  lfs f29, 0x8(r6)
  fsub f28, f28, f29
  fadd f28, f28, f28
  fabs f28, f28                 # (y1 - y2)^2
  fadd f30, f30, f28            # (x1 - x2)^2 + (y1 - y2)^2
  cmpwi r9, 0x0
  beq- StoreClosest
  fcmpo cr0, f10, f30
  blt- IncrementLoop_Closest

StoreClosest:
  cmpw r5, r11
  beq IncrementLoop_Closest
  li r9, 1
  fmr f10, f30                  # f10 = previous min distance (from last loop)
  mr r4, r5

IncrementLoop_Closest:
  addi r5, r5, 0x1
  addi r6, r6, 0x268
  cmpwi r5, 0x9
  bne+ ComputeClosest

# Finally set desired player to 0xF
# r4 is the fielder id that we want to select

AssignFielder:
  cmpwi r4, 0xff
  beq Deselect                   # this shouldn't happen but we should handle it anyway
  lhz r11, 0(r16)               # frames after contact
  lbzx r12, r23, r4             # fielder lockout for intended fielder
  cmpw r11, r12
  blt Deselect                   # if lockout not reached, don't assign
  lis r6, 0x8088
  ori r6, r6, 0xF53B            # base addr for fielder control status
  li r8, 0                      # for loop increment

Fielder_Loop:
  mulli r9, r8, 0x268
  lbzx r5, r9, r6
  cmpwi r5, 0xF                 # first we check if any fielders are selected (0xF) and set them to 0x2
  bne IncrementLoop_Fielder
  li r5, 0x2                    # a control status of 0x2 tells the game to find the correct status of the fielder; very convienent for us
  stbx r5, r9, r6

IncrementLoop_Fielder:
  addi r8, r8, 1
  cmpwi r8, 0x9
  blt Fielder_Loop
  
StoreResults:
  mulli r5, r4, 0x268           # compute offset for fielder addr
  li r7, 0xF                    # set to 0xF (selected)
  stbx r7, r5, r6
  stb r4, 0x1(r26)
  stb r4, 0x7(r26)
  b EndFake

Deselect:
  li r4, 0x0
  stb r4, 1(r15)                 # set previous select arg to 0

# EndReal just means that we didn't do a manual select, so the game will proceed normally
# we properly replace the instruction that we overwrote after restoring the stack
EndReal:
  lmw r3,0x8(r1)                # restore stack
  addi r1,r1,0x80
  mr r3, r14
  lbz r0, 0x1bd1(r6)            # Perform op where we inject
  b End

# EndFake means we will trick the game not to continue trying to assign control status after a manual select
# by setting r0 to 1 after restoring the stack, the game won't assign another fielder; nice!
EndFake:
  lmw r3,0x8(r1)                # restore stack
  addi r1,r1,0x80
  mr r3, r14
  li r0, 1

End:


