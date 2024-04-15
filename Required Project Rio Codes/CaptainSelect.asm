###########################################################
# Captain Swap
###########################################################
# Author: nuche, LittleCoaks


###########################################################
###########################################################
# Requires: "2064F67C 40820AB4"

# Inject 0x8064f67c
Start:
  lis r11,0x800f 			# Checks if on main menu rel
  subi r12,r11,0x7884 
  lhzu r10, 0 (r12) 
  cmplwi r10, 4 
  beq StartMod 				# if main menu rel, then branch to the mod.

#If not main menu, then replace normal match instructions from injection site
  addi	r6, r4, 10636
  li r5, 13
  addi r4, r3, 2320
  b End 

StartMod:
  rlwinm. r0,r30,0,19,19 		# check if start button is pressed.
  rlwinm r3,r30,0,16,31
  beq End 

# Pre-checks before switching captains.
  lis r11,0x8075 			# Check if showing player profile
  addi r12,r11,3144
  add r11,r12,r27
  lbzu r10,0x45(r11)
  cmplwi r10, 0
  bne BadSound 

  lis r11, 0x8075			# Check if cursor is on bottom half of screen
  addi r12, r11, 3144
  add r11, r12, r27
  lbzu r10, 0x41 (r11)
  cmplwi r10, 0
  bne BadSound

  lis r11, 0x8075			# Check if the cursor isn't over a captain character.
  addi r12, r11, 0x0c48
  mulli r10, r27, 0x4
  add r11, r12, r10
  lwzu r8, 0X00 (r11)
  cmpwi r8, 0x9
  bge BadSound

# Find roster spot of the cursor position, in case it's swapped.
  lis r11, 0x803c			# roster mapping address
  addi r12, r11, 0x6738
  mulli r10, r27, 0x9
  add r11, r12, r10
  li r10, 0x9
  mtctr r10				# store loop index
  li r7, 0x0
Loop1Start:
  lbz r10, 0 (r11)
  cmpw r8, r10				# compare mapping with cursor pos
  bne Loop1End 				# if not equal, then try the next index
  b MapFound 				# if equal, then found the mapping
Loop1End: 
  addi r11, r11, 0x1 			# add 1 to mapping mem address
  addi r7, r7, 0x1 			# add 1 to the roster spot
  bdnz Loop1Start
  b BadSound 				# cursor position not found
MapFound:
  nop 					# r7 now holds the roster spot of the desired captain 

# Get CharID of cursor position
  lis r11, 0x803c 			# address of character IDs
  addi r12, r11, 0x6726
  mulli r10, r27, 0x9 			# get offset for if team 2
  add r11, r12, r10 
  add r10, r11, r7			# add roster position mapping
  lbz r9, 0 (r10) 			# load char ID from address

# Check if charID in cursor pos is in the captain ID list
  lis r11, 0x8011 			# Address of list of allowed captain IDs.
  subi r12, r11, 0x7130
  li r11, 0xc				# set loop limit of 12
  mtctr r11
Loop2Start:
  lbz r11, 0 (r12) 			# load captain from list
  cmpw r9, r11 				# compare new captain with captain list
  bne Loop2End
  b CapFound
Loop2End:
  addi r12, r12, 0x1 			# add 1 to captain list index
  bdnz Loop2Start
  b BadSound #captain not found
CapFound:
  nop					# r9 now holds charID of new captain 

# Change cap ID 
  lis r11, 0x8035 			# Address where Captain ID is stored
  addi r12, r11, 0x3080
  mulli r10, r27, 0x4 			# add offset for team
  add r11, r10, r12
  stw r9, 0 (r11) 			# store new cap ID (r9) into captain address (r11)

# Swap roster IDs to put new captain in first roster spot
# Store old captain charID.
  lis r11, 0x803c 			# get address of old captain CharID
  addi r12, r11, 0x6726
  mulli r10, r27, 0x9
  add r11, r12, r10 			# add team offset
  lbz r12, 0 (r11) 			# load old captain char ID from address.
  stb r9, 0 (r11) 			# store new captain charID in first roster spot
  add r10, r11, r7 			# get address for where to store old cap ID
  stb r12, 0 (r10) 			# store old captain at old roster spot of new captain

# Swap the position mappings
  lis r11, 0x803c 			# Address of position mappings
  addi r12, r11, 0x6738
  mulli r10, r27, 0x9
  add r11, r12, r10 			# add team offset
  lbz r12, 0 (r11)			# store mapping of old captain
  stb r8, 0 (r11) 			# store current cursor position in spot 1 (this is where the new captain is on the screen)
  add r10, r11, r7
  stb r12, 0 (r10)			# store old captains mapping in new captains old roster slot


# Update chemistry values
# r11-chem table address; r12-charID on roster address; r9-chem with cap array address, r10-rosterspot index, r7 charID in roster spot
  lis r11, 0x8035			# chemistry table address
  subi r11, r11, 0x1625 
  mulli r12, r9, 0xa0 			# multiply captain ID by row length to get to offset of captain's chem table.
  add r11, r11, r12 			# add offset to get to start of captain's chem table.
  li r12, 0x8 				# loop limit (all 8 char's excluding cap)
  mtctr r12 
  lis r12,0x803c
  addi r12,r12,0x6727 			# start of roster char ID's after captain
  mulli r10, r27, 0x9 			# add offset for team 2.
  add r12, r12, r10
  lis r9, 0x803c
  addi r9, r9,0x674b 			# start of chem with captain array (after captain)
  add r9, r9, r10 			# add team offset
Loop3Start:
  lbz r7, 0(r12) 			# get char ID of roster spot
  cmpwi		r7, 0xff 		# see if roster spot is empty.
  beq		Loop3End 		# if roster spot empty, go to end to prepare for new loop
# If roster spot not empty
  add r7, r7, r11 			# store address of chem with captain into r7
  lbz r8, 0 (r7) 			# store chem into r8
  stb r8, 0(r9) 			# store chem (r8) into team chem address (r9)
Loop3End:
  addi r9, r9, 0x1 			# update chem array address by 1
  addi r12, r12, 1 			# increment roster char ID addresses
  bdnz Loop3Start 			# increment loop counter and go back to start

# Call the team logo function
  mflr r0
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)
  mr r3, r27
  lis r12, 0x8006
  ori r12, r12, 0x78cc
  mtctr r12
  bctrl
  lmw r3,0x8(r1)
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0

# Call chem star function
  mflr r0 				# backup stack frame
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)
  lis r3, 0x8035 			# set param_1
  ori r3, r3, 0x30ec
  lwz r3, 0(r3)
  mr r4, r27 				# set param_2
  lis r12, 0x806b 			# call function
  ori r12, r12, 0x4c78
  mtctr r12
  bctrl
  lmw r3,0x8(r1) 			# restore stack frame
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0

  # update state var
UpdateStateVar:
  mflr r0 				# backup stack frame
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)
  or r3, r27, r27   # set params
  li r4, 0xf
  lis r12, 0x8006 			# call function
  ori r12, r12, 0x25A4
  mtctr r12
  bctrl
  lmw r3,0x8(r1) 			# restore stack frame
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0

# Call sound effects
GoodSound:
  mflr r0 				# backup stack frame
  stw r0, 0x4(r1)
  stwu r1, -0x100(r1)
  stmw r3, 0x8(r1)
  li r3, 0x1bc 				# set param 1 (sound code)
  b PlaySound

BadSound:
  mflr r0 				# backup stack frame
  stw r0, 0x4(r1)
  stwu r1, -0x100(r1)
  stmw r3, 0x8(r1)
  li r3, 0x1ba 				# set param 1 (sound code)

PlaySound:
  lis r4, 0x800e 			# set param 2 (volume)
  ori r4, r4, 0xfba4
  lwz r4, 0(r4)
  li r5, 0x3f #set param 3
  li r6, 0x0 #set param 4
  lis r12, 0x800c 			# call function
  ori r12, r12, 0x836c 
  mtctr r12
  bctrl
  lmw r3,0x8(r1) 			# restore stack frame
  lwz r0, 0x104(r1)
  addi r1, r1,0x100
  mtlr r0

End:
