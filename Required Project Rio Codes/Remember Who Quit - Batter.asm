###########################################################
# Remmeber Who Quit - Batter
###########################################################
# Author: LittleCoaks

# Address: 0x806EDF88
# State: Game
# Port who quit a game --> 0x802EBF93


###########################################################
###########################################################

START:
  sth r4, 254(r3)                   # Replace overwritten instruction
  lis r20, 0x802E                   # Load quitter port addr
  ori r20, r20, 0xBF93
  lbz r21, 2(r20)                   # Get batter port
  stb r21, 0(r20)                   # Store to quitter addr

END:

