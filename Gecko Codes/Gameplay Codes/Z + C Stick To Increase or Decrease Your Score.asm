###########################################################
# Z + C Stick To Increase or Decrease Your Score
###########################################################
# Author: Roeming

# Address: 0x8069a15c
# State: Game

# *Z + Cstick UP to increase your score
# *Z + Cstick UP to decrease your score


###########################################################
###########################################################

.include "Common.s"

.set score1, 0x808928a4
.set score2, 0x808928ca
.set BUTTON_Z, 0x10

    # short, offset 0x20 to next 
.set newlyPressedButtons, 0x803c77ba

    # byte, offset 8 to next one
.set cStick, 0x802e9f45

    # bytes
.set ports, 0x80892A78

    # ints
.set teamInds, 0x80892998

    # ints
.set battingFielding, 0x80892990

    # byte
.set bottomInningBool, 0x8089294d

  # 0x80 is cstick neutral
.set CSTICK_UP_THRESHOLD, 0x90
.set CSTICK_DOWN_THRESHOLD, 0x70

  # loop through batter/pitcher
  li r5, 0x2
  mtctr r5

  # load the batter/pitcher port index
  # load it so we can use lwzu, and don't have to manually index
  lis r9, (battingFielding-0x4)@ha
  addi r9, r9, (battingFielding-0x4)@l

  # this is the main way we can line up the player with their score
  # the first loop around, we'll deal with the current batter, then the second loop is the pitcher
  # the score we will update is entirely based on this as our index
  # we just flip it each loop, and it correctly aligns with our current players score
  # batter score = scores[bottomInningBool]
  # pitcher score = scores[bottomInningBool^1]
  lis r5, bottomInningBool@ha
  lbz r7, bottomInningBool@l(r5)
loop:
  # r8 holds the "player"
  # the first time around, its the batter, then the pitcher
  # this is just a 0,1 for which port to look into
  lwzu r8, 0x4(r9)

  lis r3, ports@ha
  addi r3, r3, ports@l
  slwi r6, r8, 2  # shift player by 2 to become an int offset
  lwzx r4, r6, r3 # r4 holds the port
  cmplwi r4, 4    # if port is greater than or equal to 4, we skip. ai or other
  bge inc_loop

  lis r3, newlyPressedButtons@ha
  addi r3, r3, newlyPressedButtons@l
  slwi r6, r4, 5          # shift port by 5 to essentially multiply by 0x20 
  lhax r6, r3, r6         # r6 holds buttons that were pressed this frame
  andi. r6, r6, BUTTON_Z  # & with z, then compare to 0
  # make sure the z button was pressed this frame
  beq inc_loop

  # r4 has the port, and we've guaranteed that z was pressed this frame

  lis r6, cStick@ha
  addi r6, r6, cStick@l
  slwi r5, r4, 3 # shift port by 3 to mult by 8, get offset for this players cstick
  lbzx r5, r6, r5
  # r5 now holds the current cStick y value

  cmpwi r5, 0 # check for cstick of 0, means no input. maybe unnecessary
  beq inc_loop

  mulli r6, r7, (score2-score1) # mult by half-inning to get the correct offset for this score
  lis r3, score1@ha
  addi r3, r3, score1@l

  # explaining lhaux
  # add r6, score offset, onto the pointer to the scores, r3
  # load r6 with the current players score
  # at the same time, r3 gets updated to add r6, so now it points to this players score
  lhaux r6, r3, r6
  # r6 holds the score
  # r3 now holds the score pointer to write to

  # check cstick up
  cmpwi r5, CSTICK_UP_THRESHOLD
  blt check_cstick_down # not held high enough

  cmpwi r6, 99 # if score is 99, don't go past max
  bge inc_loop

  # increment the score, and then store it
  addi r6, r6, 1
  b store_val

check_cstick_down:
  cmpwi r5, CSTICK_DOWN_THRESHOLD
  bgt inc_loop # not held down enough

  cmpwi r6, 0 # check if the score is 0, if it is, don't decrement
  ble inc_loop

  # decrement the score, then fall through and store it
  subi r6, r6, 1

store_val:
  sth r6, 0x0(r3)

inc_loop:
  # r7 is the half inning, but also it helps up know which score to store to
  # flip it each loop
  xori r7, r7, 0x1 
  bdnz loop

  # cleanup, the instruction we messed with
  mflr r0
  