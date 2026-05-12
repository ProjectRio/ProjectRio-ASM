###########################################################
# Store Port Info
###########################################################
# Author: LittleCoaks

# Address: 0x806706B8
# State: Game

###########################################################
###########################################################

START:
  lis r31, 0x8089               # Check if P1 is controlling pitcher
  ori r31, r31, 0x3928
  cmpw r4, r31                  # If r4 == r31, P1 controlls the pitcher
  lis r31, 0x802E               # Load init port info addr
  ori r31, r31, 0xBF91
  beq- P1_FIELDING

P1_BATTING:
  lbz r3, 0(r31)                # Move P1 info to Batter Port addr
  stb r3, 4(r31)
  lbz r3, 1(r31)                # Move Px info to Fielder Port addr
  stb r3, 3(r31)
  b END

P1_FIELDING:
  lbz r3, 1(r31)                # Move Px info to Batter Port addr
  stb r3, 4(r31)
  lbz r3, 0(r31)                # Move P1 info to Fielder Port addr
  stb r3, 3(r31)

END:
  lis r3, 0x8089                # Replace overwritten instruction

