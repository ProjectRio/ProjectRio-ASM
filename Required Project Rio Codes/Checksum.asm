###########################################################
# Checksum
###########################################################
# Author: LittleCoaks



###########################################################
###########################################################

# this checksum will check a few variables and make sure that they're the same between clients on netplay
# if they all are, we can safely assume that the client's game's are in sync
# 0x802EBFB8 - checksum

# Requires: 2069a160 3c80800f
# Inject: 0x8069a160
Start:
  mflr r0                   # backup registers
  stw r0, 0x4(r1)
  stwu r1, -0x100(r1)
  stmw r3, 0x8(r1)


# Sum in r4
# Addr to r5
Init_Registers:
  li r4, 0

Sum_Values:
  lis r5, 0x8089                # current game state
  ori r5, r5, 0x2aaa
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # previous game state
  ori r5, r5, 0x2aab
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # contact made
  ori r5, r5, 0x09a1
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # pickoff attempt
  ori r5, r5, 0x2857
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8036                # game is live
  ori r5, r5, 0xF3A9
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # play is ready to start
  ori r5, r5, 0x09AA
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8087                # is replay
  ori r5, r5, 0x2540
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # batter roster id
  ori r5, r5, 0x0971
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # inning
  ori r5, r5, 0x28A3
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # half inning
  ori r5, r5, 0x294D
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # balls
  ori r5, r5, 0x296F
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # strikes
  ori r5, r5, 0x296B
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # outs
  ori r5, r5, 0x2973
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # p1 stars
  ori r5, r5, 0x2AD6
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # p2 stars
  ori r5, r5, 0x2AD7
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # is star chance
  ori r5, r5, 0x2AD8
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # chem links on base
  ori r5, r5, 0x09BA
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8088                # runner on 1st
  ori r5, r5, 0xF09D
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8088                # runner on 2nd
  ori r5, r5, 0xF1F1
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8088                # runner on 3rd
  ori r5, r5, 0xF345
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # outs during play
  ori r5, r5, 0x38AD
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # hit by pitch
  ori r5, r5, 0x09A3
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # final result
  ori r5, r5, 0x3BAA
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # away team runs
  ori r5, r5, 0x28A4
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # home team runs
  ori r5, r5, 0x28CA
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # batter roster id
  ori r5, r5, 0x0971
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # pithcher roster id
  ori r5, r5, 0x0AD9
  lbz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # Ball pos X
  ori r5, r5, 0x0B38
  lwz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # Ball Pos Y
  ori r5, r5, 0x0B3C
  lwz r5, 0(r5)
  add r4, r5, r4
  lis r5, 0x8089                # Ball Pos Z
  ori r5, r5, 0x0B40
  lwz r5, 0(r5)
  add r4, r5, r4

# 0x802EBFB8 - checksum
Store_Sum:
  lis r5, 0x802E
  ori r5, r5, 0xBFB8
  stw r4, 0(r5)


End:
  lmw r3,0x8(r1)            # restore registers
  lwz r0, 0x104(r1)
  addi r1,r1, 0x100
  mtlr r0
  lis r4, 0x800F            # replace instruction

