###########################################################
# Manual Fielder Select v3.2
###########################################################
# Authors: PeacockSlayer, LittleCoaks

# Control Status: 
# 0x8088F53B
# 0x8088F773
# 0x8088FA0B
# 0x8088FC73
# 0x8088FEDB
# 0x80890143
# 0x808903AB
# 0x80890613
# 0x8089087B
# Locks are all -0xF2 away from control status

###################################################################################
# First nop this line so we can use the memory address as a debouncer
# We only want to capture the input the instant its pushed. We don't want to increment
# our fielder if we hold it down
# 
# Mem address 0x80892897 (set breakpoint to find instr, I nopped it early)
###################################################################################



###########################################################
###########################################################
# Inject: 0x8067ac00
loc_START:
  sth r0, 0x148(r5)             # Perform op where we inject
  mr r14, r0

  mflr r0                     # backup stack frame
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)

  lis r18, 0x8023             # Setup r18 with debounce byte (tells us if button has been unpressed)
  addi r18, r18, 0x6D30       # Clear when no button is pressed, or button other than swap was pressed
  cmpwi r14, 0x0
  bne- loc_IsFielderFielding
  stb r15, 0x6D39(r18)
  b loc_END                        # Return if input is zero (most cycles it will be)

loc_IsFielderFielding:        # Get IsFielder Fielding Value. Return if pitching
  lis r6, 0x8089
  ori r6, r6, 0x389B
  lbz r6, 0(r6)
  cmpwi r6, 0x0
  bne- loc_CheckButton_Y
  b loc_END 

# convert button input into arg and store in r24
# 0x1: Forward
# 0x2: SS
# 0x3: Backwards
# 0x4: 2nd
# 0x5: Closest fielder
# 0xFF: Nothing
#
# r14 Is X or Y pressed
# r19 is Z pushed (only read if X or Y was pushed)
# set r24 to arg for type of manual selection to be made
loc_CheckButton_Y:
  li r24, 0xFF
  andi. r15, r14, 0x800
  cmpwi r15, 0x800
  bne- loc_CheckButton_X
  li r24, 0x1

loc_CheckButton_X:
  andi. r15, r14, 0x400
  cmpwi r15, 0x400
  bne- loc_CheckButton_Z
  li r24, 0x3

loc_CheckButton_Z:
  cmpwi r24, 0xFF
  beq- loc_CheckButton_R
  andi. r15, r14, 0x10
  cmpwi r15, 0x10
  bne- loc_CheckButton_R
  addi r24, r24, 0x1

loc_CheckButton_R:
  andi. r15, r14, 0x20
  cmpwi r15, 0x20
  bne- loc_CheckValidArgs
  li r24, 0x5

loc_CheckValidArgs:
  li r20, 0xFF                # Set r20 to 0xFF. Means no one is selected. Will set later if someone is
  cmpwi r24, 0xFF             # Not a valid input for character ctrl status update. Clear button state
  bne- loc_IsButtonReleased
  li r15, 0x0                 # debounce set to 0 (not held)
  stb r15, 0x6D39(r18)
  b loc_END

# return if button has not been released
loc_IsButtonReleased:
  lbz r15, 0x6D39(r18)          # check if it's first button press
  cmpwi r15, 0x1
  bne- loc_SetLockStatus
  b loc_END 

# Add A check back in
# For loop over all fielders. Set ctrl status lock=1 (selected =2 comes later)
# Clear any Fielder with 0xF or 0x15 status (unselect).
# If a fielder is holding the ball set a var to clear all status and escape
# Note which player is currently locked (0x2)
loc_SetLockStatus:
  mulli r15, r23, 0x268         # for loop i at r23
  lis r16, 0x8088               # Set up control status addr at r16
  addi r17, r15, 0x7A9D
  addi r17, r17, 0x7A9E
  or r16, r16, r17
  lbz r18, 0(r16)               # control status in r18
  cmpwi r18, 0xA
  bne- loc_IsStatusUnselect
  b loc_END 

loc_IsStatusUnselect:
  cmpwi r18, 0x15
  bne- loc_IsStatusUnselectF
  b loc_ClearStatus

loc_IsStatusUnselectF:
  cmpwi r18, 0xF
  bne- loc_FindLockedPlayer

loc_ClearStatus:
  li r18, 0x0
  stb r18, 0(r16)

loc_FindLockedPlayer:
  lbz r18, -0xF2(r16)
  cmpwi r18, 0x2
  bne- loc_LockFielderAndLoop
  mr r20, r23                   # r20 contains locked character position index

loc_LockFielderAndLoop:
  li r18, 0x1
  stb r18, -242(r16)
  addi r23, r23, 0x1
  cmpwi r23, 0x9
  blt+ loc_SetLockStatus


# r26 will hold fielder we are selecting
# r20 holds the player that is currently selected (0xFF if none)
# Perform lookup based on r24 in fixed array of 0-1-2-3-5-4-6-7-8 (P-C-1-2-SS-3-LF-CF-RF)
# 
# IF r24 is 0x2: set r26 to SS (5)
# ELSE IF r24 is 0x4: set r26 to 2nd (3)
# ELSE IF r24 is 0x5: set r26 to closest fielder
# ELSE IF r20 is 0xFF: NO FIELDER IS LOCKED. LOOK AT r24 to determine if we want P or RF
# IF r20 is any other value: Iterate through array and find value. Select fielder to left or right based on r24

# 1.
loc_Select_SS:
  cmpwi r24, 0x2
  bne- loc_Select_2B
  li r26, 0x5
  b loc_AssignFielder

# 2.
loc_Select_2B:
  cmpwi r24, 0x4
  bne- loc_Select_Closest
  li r26, 0x3
  b loc_AssignFielder

# 3.
loc_Select_Closest:
  cmpwi r24, 0x5
  bne- loc_IsFirstSelection
  li r24, 0x0                   # r24 = for loop increment
  li r26, 0x0                   # r26 = min distance fielder
  lis r18, 0x8088               # r18 = P position addr (ba = 0x8088F368)
  ori r18, r18, 0xF368
  lis r19, 0x8089               # r19 = Ball position addr (ba = 0x80890B38)
  ori r19, r19, 0xB38

loc_Compute_Closest:
  lfs f28, 0(r19)
  lfs f29, 0(r18)
  fsub f30, f28, f29
  fadd f30, f30, f30
  fabs f30, f30
  lfs f28, 8(r19)
  lfs f29, 8(r18)
  fsub f28, f28, f29
  fadd f28, f28, f28
  fabs f28, f28
  fadd f30, f30, f28             # f30 = current min distance
  cmpwi r24, 0x0
  beq- loc_StoreClosest
  fcmpo cr0, f10, f30
  blt- loc_IncrementLoop_Closest

loc_StoreClosest:
  fmr f10, f30                   # f10 = previous min distance
  mr r26, r24

loc_IncrementLoop_Closest:
  addi r24, r24, 0x1
  addi r18, r18, 0x268
  cmpwi r24, 0x9
  bne+ loc_Compute_Closest
  b loc_AssignFielder

# 4.
loc_IsFirstSelection:
  cmpwi r20, 0xFF
  bne- loc_FindFielder

loc_Select_P:
  cmpwi r24, 0x1
  bne- loc_Select_C
  li r26, 0x0
  b loc_AssignFielder

loc_Select_C:
  cmpwi r24, 0x3
  bne- loc_FindFielder
  li r26, 0x8
  b loc_AssignFielder

# 5.
loc_FindFielder:
  lis r18, 0x8023               # Set up fixed array addr
  ori r18, r18, 0xDA60
  mr r17, r18                   # move array addr to r17
  li r23, 0x0                   # for loop incrementor

loc_StartLoop_Fielders:
  lbz r6, 0(r18)                # Load Fielder array val to r6
  cmpw r6, r20                  # is the fielder array value the locked player id?
  bne- loc_IncrementLoop_Fielders

loc_ShouldLoopForwards:
  cmpwi r24, 0x1                # 1 = loop forwards
  bne- loc_ShouldLoopBackwards
  addi r18, r18, 0x1
  b loc_ExitLoop_Fielders

loc_ShouldLoopBackwards:
  cmpwi r24, 0x3                # loop backwards
  bne- loc_IncrementLoop_Fielders
  subi r18, r18, 0x1
  b loc_ExitLoop_Fielders

loc_IncrementLoop_Fielders:
  addi r18, r18, 0x1
  addi r23, r23, 0x1
  cmpwi r23, 0x9
  blt+ loc_StartLoop_Fielders

loc_ExitLoop_Fielders:
  cmpw r18, r17                 # is current addr above the minimum addr
  bge- loc_GetArrayIndexVal     # Branch if above lower bound
  addi r18, r18, 0x9
  b loc_LoadArgFromArray        # READY TO READ

loc_GetArrayIndexVal:
  addi r17, r17, 0x9
  cmpw r18, r17
  blt- loc_LoadArgFromArray     # Branch if below upper bound
  subi r18, r17, 0x9

loc_LoadArgFromArray:
  lbz r26, 0(r18)

# Finally set desired player to 0xF. Reset vars. Return
loc_AssignFielder:
  mulli r15, r26, 0x268         # get addr for desired fielder
  lis r16, 0x8088
  addi r17, r15, 0x7A9D
  addi r17, r17, 0x7A9E
  or r16, r16, r17
  li r23, 0xF                   # set to 0xF (selected)
  stb r23, 0(r16)
  li r23, 0x2
  stb r23, -0xF2(r16)           # set lock to selected
  lis r16, 0x8089               # Finally set the hand indicator + lock
  ori r16, r16, 0x2801
  stb r26, 0(r16)
  lis r18, 0x8023               # Set button state to 1 meaning we need to see a depress before moving again
  addi r18, r18, 0x6D30
  li r15, 0x1
  stb r15, 0x6D39(r18)

loc_END:
  lmw r3,0x8(r1)                # restore stack
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0



#############################################################################################
#############################################################################################
# Release Locks AND ctrl status
# (0x00) r14-r24 (RESET)

# Inject: 0x8069797c
loc_START:
  stb r6, 0xE1(r29)
  stb r6, 0x1D3(r29)            # restore original instruction

END:



#############################################################################################
#############################################################################################
# Prevents any character control status from turning to 0xF unless player is selected (lock==2)
# (0x0F) r14-r24, r26

# Inject: 0x80677950
loc_START:
  lbz r14, 0x00E1 (r10)
  cmpwi r14, 1
  li r14, 0
  beq loc_END
  stb	r8, 0x01D3 (r10)          # restore original instruction

loc_END:



#############################################################################################
#############################################################################################
# Releases all locks once ball is picked up or collected 0xA
# (0x0A) r14-r24, r26

# Inject: 0x806663c8
loc_START:
  li r14, 0x0

loc_STARTLOOP:
  mulli r15, r14, 0x268
  lis r16, 0x8088
  addi r17, r15, 0x7A9D
  addi r17, r17, 0x7A9E
  or r16, r16, r17
  li r18, 0x0
  stb r18, -0xF2(r16)
  addi r14, r14, 0x1
  cmpwi r14, 0x9
  blt+ loc_STARTLOOP
  stb	r0, 0x01D3 (r28)          # restore original instruction



#############################################################################################
#############################################################################################
# HAND
# Run through all chars and set hand to char that has lock == 2 (selected)
# r14-r24

# Write: 0x802EC000
loc_START:
  mflr r0                   # backup stack
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)

  li r9, 0xF                # init parameters
  lis r4, 0x8089
  ori r4, r4, 0x2750
  li r11, 0x0
  li r5, 0x0

loc_STARTLOOP:
  mulli r5, r11, 0x268
  lis r6, 0x8088
  addi r8, r5, 0x7A9D
  addi r8, r8, 0x7A9E
  or r6, r6, r8
  lbz r10, -0xF2(r6)
  cmpwi r10, 0x2
  bne- loc_IncrementLoop

loc_SetHand:
  sth r11, 0xB0(r4)
  stb r9, 0(r6)
  b loc_END

loc_IncrementLoop:
  addi r11, r11, 0x1
  cmpwi r11, 0x9
  blt+ loc_STARTLOOP
  sth r20, 0xB0(r4)

loc_END:
  lmw r3,0x8(r1)
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0
  blr



########################################
# Injections branch to the write above #
########################################

# Inject: 0x80677920
loc_START:
  mr r20, r3                # move arg so stack can be backed up
  mr r21, r0
  mflr r0                   # backup stack
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)
  lis r9, 0x802E            # load function to ctr & branch
  ori r9, r9, 0xC000
  mtctr r9
  bctrl
  lmw r3,0x8(r1)            # restore stack
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0
  mr r0, r21

END:


# Inject: 0x80672b88
loc_START:
  mr r20, r0                # move arg so stack can be backed up
  mflr r0                   # backup stack
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)
  lis r9, 0x802E            # load function to ctr & branch
  ori r9, r9, 0xC000
  mtctr r9
  bctrl
  lmw r3,0x8(r1)            # restore stack
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0

END:


# Inject: 0x8067a684
loc_START:
  mr r20, r0                # move arg so stack can be backed up
  mflr r0                   # backup stack
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)
  lis r9, 0x802E            # load function to ctr & branch
  ori r9, r9, 0xC000
  mtctr r9
  bctrl
  lmw r3,0x8(r1)            # restore stack
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0

END:


# Inject: 0x8067aecc
loc_START:
  mr r20, r0                # move arg so stack can be backed up
  mflr r0                   # backup stack
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)
  lis r9, 0x802E            # load function to ctr & branch
  ori r9, r9, 0xC000
  mtctr r9
  bctrl
  lmw r3,0x8(r1)            # restore stack
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0

END:



#############################################################################################
#############################################################################################
# [TEMPLATE]
# 
# ORIGINAL INSTR
# lis r15, 0x8088 
# ori r15, r15, 0xf368 
# addi r16, r15, 0x15A8 
# cmpw r18, r15 
# blt 0x1C
# cmpw r18, r16 
# bgt 0x14
# lbz r14, 0xE1 (dest_reg)
# cmpwi r14, 2 
# bne 0x8 //ESCAPE
# cmpwi lbz_reg, 0xA
# beq 0xC
# li load_reg, 0xF
# stb load_reg, ORIGINAL OFFSET + dest_REG


# Inject: 0x8069628c
loc_START:
  lbz r0, 0x1D3(r28)
  lis r15, 0x8088
  ori r15, r15, 0xF368
  addi r16, r15, 0x15A8
  cmpw r28, r15
  blt- loc_END
  cmpw r28, r16
  bgt- loc_END
  lbz r14, 0xE1(r28)
  cmpwi r14, 0x2
  bne- loc_END
  cmpwi r0, 0xA
  beq- loc_END
  li r0, 0xF
  stb r0, 0x1D3(r28)

loc_END:


# Inject: 0x80692224
loc_START:
  lbz r4, 0x01D3(r31)
  lis r15, 0x8088
  ori r15, r15, 0xF368
  addi r16, r15, 0x15A8
  cmpw r31, r15
  blt- loc_END
  cmpw r31, r16
  bgt- loc_END
  lbz r14, 0xE1(r31)
  cmpwi r14, 0x2
  bne- loc_END
  cmpwi r4, 0xA
  beq- loc_END
  li r4, 0xF
  stb r4, 0x1D3(r31)

loc_END:


# Inject: 0x80685060
loc_START:
  lbz r8, 0x01D3(r6)
  lis r15, 0x8088
  ori r15, r15, 0xF368
  addi r16, r15, 0x15A8
  cmpw r6, r15
  blt- loc_END
  cmpw r6, r16
  bgt- loc_END
  lbz r14, 0xE1(r6)
  cmpwi r14, 0x2
  bne- loc_END
  cmpwi r8, 0xA
  beq- loc_END
  li r8, 0xF
  stb r8, 0x1D3(r6)

loc_END:



#############################################################################################
#############################################################################################
# Write fixed array of fielder inputs to static memory

# Gecko Writes:
  0023da60 00000000
  0623da61 00000008
  01020304 05060708

