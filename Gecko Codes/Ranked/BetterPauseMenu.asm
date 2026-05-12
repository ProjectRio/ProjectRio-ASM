###########################################################
# Better Pause Menu
###########################################################
# Author: LittleCoaks


###########################################################
###########################################################
# Requires: "206ee470 38600002"
# Inject: 0x806EE474
START:
	li r3, 3				# some weird overlay stuff
	stb r3, 0x1D9(r4)
    li r3, 4
	stb	r3, 0x01D1 (r4)
	li r3, 2

# Requires: "206ECAA8 38800002"
# Inject: 0x806ECAA8
START:
	lbz r4, 4(r3)           # player who paused's inputs
	andi. r4, r4, 0x2       # check if they pressed B
	cmpwi r4, 2
	beq END                 # keep r4 as 2, which sends back to normal menu
	li r4, 2				# some weird overlay stuff
	stb r4, 0x1D9(r3)
	li r4, 3                # set r4 to 3, which is back to game

END:

