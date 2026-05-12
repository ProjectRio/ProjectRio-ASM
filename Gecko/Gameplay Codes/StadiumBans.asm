###########################################################
# Stadium Bans
###########################################################
# Author: LittleCoaks



###########################################################
###########################################################
# Requires: 2065074C 80630000

# Inject: 0x8065074C
START:
  mflr r0                   # backup registers
  stw r0, 0x4(r1)
  stwu r1,-0x100(r1)
  stmw r3,0x8(r1)

CHECK_FOR_X:
  lis r4, 0x8035
  ori r4, r4, 0x30CC
  lbz r5, 0(r4)             # player 1 inputs
  lbz r6, 0x8(r4)           # other player inputs
  andi. r5, r5, 0x4         # are either of them pressing X
  andi. r6, r6, 0x4
  or r5, r5, r6
  cmpwi r5, 0
  beq END                   # if neither pressing X, end function

CLEAR_ICON:
  lis r5, 0x803B            # starting addr for icons
  ori r5, r5, 0x6EF7
  lis r6, 0x8075
  ori r6, r6, 0x0C37
  lbz r6, 0(r6)             # get current stadium being hovered over
  mulli r6, r6, 0xC0
  add r5, r5, r6
  li r6, 0
  stb r6, 0(r5)             # remove icon


END:
  lmw r3,0x8(r1)            # restore registers
  lwz r0, 0x104(r1)
  addi r1,r1,0x100
  mtlr r0
  lwz	r3, 0 (r3)          # replace instruction

