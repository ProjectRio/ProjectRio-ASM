###########################################################
# Store Port Info
###########################################################
# Author: LittleCoaks

# Address: 0x8063F14C
# State: Game
# *Clear port info after game ends

###########################################################
###########################################################

START:
  li r3, 0x0                    # Clear r3
  lis r5, 0x802E                # Load port info addr
  ori r5, r5, 0xBF90
  stb r3, 1(r5)                 # Clear addrs   
  sth r3, 2(r5)
  sth r3, 4(r5)

END:

