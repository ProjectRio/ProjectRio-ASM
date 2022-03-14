###########################################################
# Load Savestate
###########################################################
# Author: LittleCoaks, bko



###########################################################
###########################################################
# Requires: 206aa1f0 889b011e

# Inject: 0x806aa1f0
START:
  mflr r0                   # backup registers
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)
  lbz	r4, 0x011E (r27)    # grab Game State
  cmpwi r4, 2               # check if in a play
  bne END
  lis r6, 0x802E            # load p1 input addr to r6
  ori r6, r6, 0x9F40
  li r8, 0                  # initialize for loop incrementer

FOR_LOOP:
  lbz r7, 1(r6)             # grab player inputs
  andi. r7, r7, 0xD0        # are they pressing L + Z
  cmpwi r7, 0xD0
  beq LOAD_STATE
  addi r8, r8, 1
  addi r6, r6, 8
  cmpwi r8, 4
  bne FOR_LOOP
  b END

LOAD_STATE:
  li r4, 0x7
  stb r4, 0x011E(r27)    # write Game State

END:
  lmw r3,0x8(r1)            # restore registers
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0
  lbz	r4, 0x011E (r27)    # replace instruction

