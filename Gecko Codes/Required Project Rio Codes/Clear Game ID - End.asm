###########################################################
# Clear Game ID - End
###########################################################
# Author: LittleCoaks

# Address: 0x8069AB2C
# State: Game

###########################################################
###########################################################

START:
  stb r0, 293(r5)               # Replace overwritten instruction
  lis r18, 0x802E               # Load GameID addr
  ori r18, r18, 0xBF8C
  li r3, 0x0
  stw r3, 0(r18)                # Set to 0

END:

