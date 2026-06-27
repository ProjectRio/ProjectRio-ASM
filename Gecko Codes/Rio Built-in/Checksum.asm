###########################################################
# Checksum
###########################################################
# Author: LittleCoaks

# Address: 0x8000928c

###########################################################
###########################################################

# this checksum will check a few variables and make sure that they're the same between clients on netplay
# if they all are, we can safely assume that the client's game's are in sync
# 0x802EBFB8 - checksum address

# TODO: Finish the transition to using the Common.s macros

.include "Common.s"

.set checksum, r14
.set val, r15
.set i, r16     # loop counter
.set current_scene, 0x800E877E
.set previous_scene, 0x800E8782
.set ports, 0x800E874C
.set teams, 0x803C6726
.set superstar_icons, 0x8035323B

.set checksum_addr, 0x802EBFB8

Start:
  backup_nv

Init_Registers:
  li checksum, 0
  li i, 0

Menu_State:
  loadhz val, current_scene
  add checksum, val, checksum
  loadhz val, previous_scene
  add checksum, val, checksum
  loadhz val, ports
  add checksum, val, checksum

Team_Info:
  load val, teams
  lbzx val, i, val
  add checksum, val, checksum
  add checksum, i, checksum
  load val, superstar_icons
  lbzx val, i, val
  add checksum, val, checksum
  add checksum, i, checksum
  addi i, i, 1
  cmpwi i, 18
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
  load r15, checksum_addr
  stw r14, 0(r15)


End:
  restore_nv
  cmplwi r24, 0x0   # restore original instruction
