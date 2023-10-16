###########################################################
# Checksum
###########################################################
# Author: LittleCoaks



###########################################################
###########################################################

# this checksum will check a few variables and make sure that they're the same between clients on netplay
# if they all are, we can safely assume that the client's game's are in sync
# 0x802EBFB8 - checksum

# Inject: 0x8000928c
Start:
  stwu r1, -0x50(r1)            # backup registers 14-31
  stmw r14, 0x8(r1)

# Sum in r14
# Addr to r15
# for loop index to r16
Init_Registers:
  li r14, 0
  li r16, 0

Menu_State:
  lis r15, 0x800E                # current scene
  ori r15, r15, 0x877E
  lhz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x800E                # previous scene
  ori r15, r15, 0x8782
  lhz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x800E                # ports
  ori r15, r15, 0x874C
  lhz r15, 0(r15)
  add r14, r15, r14

Team_Info:
  lis r15, 0x803C                # teams
  ori r15, r15, 0x6726
  lbzx r15, r16, r15
  add r14, r15, r14
  add r14, r16, r14
  lis r15, 0x8035                # superstar icons
  ori r15, r15, 0x323B
  lbzx r15, r16, r15
  add r14, r15, r14
  add r14, r16, r14
  addi r16, r16, 1
  cmpwi r16, 18
  blt Team_Info

InGame_State:
  lis r15, 0x8089                # current game state
  ori r15, r15, 0x2aaa
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # previous game state
  ori r15, r15, 0x2aab
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # contact made
  ori r15, r15, 0x09a1
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # pickoff attempt
  ori r15, r15, 0x2857
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8036                # game is live
  ori r15, r15, 0xF3A9
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # play is ready to start
  ori r15, r15, 0x09AA
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8087                # is replay
  ori r15, r15, 0x2540
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # batter roster id
  ori r15, r15, 0x0971
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # inning
  ori r15, r15, 0x28A3
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # half inning
  ori r15, r15, 0x294D
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # balls
  ori r15, r15, 0x296F
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # strikes
  ori r15, r15, 0x296B
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # outs
  ori r15, r15, 0x2973
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # p1 stars
  ori r15, r15, 0x2AD6
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # p2 stars
  ori r15, r15, 0x2AD7
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # is star chance
  ori r15, r15, 0x2AD8
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # chem links on base
  ori r15, r15, 0x09BA
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8088                # runner on 1st
  ori r15, r15, 0xF09D
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8088                # runner on 2nd
  ori r15, r15, 0xF1F1
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8088                # runner on 3rd
  ori r15, r15, 0xF345
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # outs during play
  ori r15, r15, 0x38AD
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # hit by pitch
  ori r15, r15, 0x09A3
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # final result
  ori r15, r15, 0x3BAA
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # away team runs
  ori r15, r15, 0x28A4
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # home team runs
  ori r15, r15, 0x28CA
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # batter roster id
  ori r15, r15, 0x0971
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # pitcher roster id
  ori r15, r15, 0x0AD9
  lbz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # Ball pos X
  ori r15, r15, 0x0B38
  lwz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # Ball Pos Y
  ori r15, r15, 0x0B3C
  lwz r15, 0(r15)
  add r14, r15, r14
  lis r15, 0x8089                # Ball Pos Z
  ori r15, r15, 0x0B40
  lwz r15, 0(r15)
  add r14, r15, r14

# 0x802EBFB8 - checksum
Store_Sum:
  lis r15, 0x802E
  ori r15, r15, 0xBFB8
  stw r14, 0(r15)


End:
  lmw r14, 0x8(r1)              # restore registers 14-31
  addi r1,r1, 0x50
  cmplwi r24, 0x0               # replace instruction


