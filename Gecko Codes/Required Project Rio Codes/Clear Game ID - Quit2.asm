###########################################################
# Clear Game ID 2
###########################################################
# Author: LittleCoaks

# Address: 0x806EDF8C
# State: Game
# *Clear Game ID when exiting mid-game

###########################################################
###########################################################


START:
  stb r0, 466(r31)              # Replace overwritten instruction
  lis r8, 0x802E                # Load GameID addr
  ori r8, r8, 0xBF8C
  li r0, 0x0
  stw r0, 0(r8)                 # Set to 0

END:
