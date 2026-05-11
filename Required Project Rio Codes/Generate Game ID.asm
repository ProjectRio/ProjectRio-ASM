###########################################################
# Generate Game ID
###########################################################
# Author: LittleCoaks

# Address: 0x80042CCC
# *Generate GameID when Start Game is pressed

###########################################################
###########################################################


START:
  lis r4, 0x802E                # Load GameID addr
  ori r4, r4, 0xBF8C
  mftbl r3                      # RNG number
  stw r3, 0(r4)
  lis r4, 0x800F                # Replace overwritten instruction

END:

