###########################################################
# GameID
###########################################################
# Author: LittleCoaks



###########################################################
###########################################################
# Generate GameID when Start Game is pressed

# Inject: 0x80042CCC
START:
  lis r4, 0x802E                # Load GameID addr
  ori r4, r4, 0xBF8C
  mftbl r3                      # RNG number
  stw r3, 0(r4)
  lis r4, 0x800F                # Replace overwritten instruction

END:



###########################################################
###########################################################
# Clear Game ID when exiting mid-game
# Requires: 206ED704 981F01D2

# Inject: 0x806ED704
START:
  stb r0, 466(r31)              # Replace overwritten instruction
  lis r8, 0x802E                # Load GameID addr
  ori r8, r8, 0xBF8C
  li r0, 0x0
  stw r0, 0(r8)                 # Set to 0

END:


# Inject: 0x806EDF8C
START:
  stb r0, 466(r31)              # Replace overwritten instruction
  lis r8, 0x802E                # Load GameID addr
  ori r8, r8, 0xBF8C
  li r0, 0x0
  stw r0, 0(r8)                 # Set to 0



###########################################################
###########################################################
# Clear Game ID when returning to main menu after game ends
# Requires: 2069AB2C 98050125

# Inject: 0x8069AB2C
START:
  stb r0, 293(r5)               # Replace overwritten instruction
  lis r18, 0x802E               # Load GameID addr
  ori r18, r18, 0xBF8C
  li r3, 0x0
  stw r3, 0(r18)                # Set to 0

END:

