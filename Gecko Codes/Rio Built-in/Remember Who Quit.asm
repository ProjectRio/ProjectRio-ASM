###########################################################
# Remmeber Who Quit - Batter
###########################################################
# Author: LittleCoaks
# State: Game


###########################################################
###########################################################
# Port who quit a game --> 0x802EBF93

# Address: 0x806EDF88
START:
  sth r4, 254(r3)                   # Replace overwritten instruction
  lis r20, 0x802E                   # Load quitter port addr
  ori r20, r20, 0xBF93
  lbz r21, 2(r20)                   # Get batter port
  stb r21, 0(r20)                   # Store to quitter addr

END:

###########################################################
# Remmeber Who Quit - Fielder
###########################################################
# Port who quit a game --> 0x802EBF93

# Address: 0x806ED700
START:
  sth r4, 254(r3)                   # Replace overwritten instruction
  lis r20, 0x802E                   # Load quitter port addr
  ori r20, r20, 0xBF93
  lbz r21, 1(r20)                   # Get fielder port
  stb r21, 0(r20)                   # Store to quitter addr

END:


